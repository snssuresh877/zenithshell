#!/usr/bin/env python3
import sys, os, subprocess, json, urllib.request, mimetypes
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, Pango

def show_error(msg):
    subprocess.run(["notify-send", "-u", "critical", "Zenith AI Error", msg])
    sys.exit(1)

def show_result(title, content):
    win = Gtk.Window(title=title)
    win.set_default_size(700, 600)
    win.set_position(Gtk.WindowPosition.CENTER)
    
    # Enable dark mode
    settings = Gtk.Settings.get_default()
    settings.set_property("gtk-application-prefer-dark-theme", True)
    
    scrolled = Gtk.ScrolledWindow()
    scrolled.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
    
    textview = Gtk.TextView()
    textview.set_editable(False)
    textview.set_cursor_visible(False)
    textview.set_wrap_mode(Gtk.WrapMode.WORD)
    textview.set_pixels_above_lines(4)
    textview.set_pixels_below_lines(4)
    textview.set_pixels_inside_wrap(2)
    textview.set_left_margin(16)
    textview.set_right_margin(16)
    textview.set_top_margin(16)
    textview.set_bottom_margin(16)
    
    # Modern font
    context = textview.get_pango_context()
    font_desc = Pango.FontDescription.from_string("sans-serif 11")
    textview.modify_font(font_desc)
    
    buffer = textview.get_buffer()
    buffer.set_text(content)
    
    scrolled.add(textview)
    win.add(scrolled)
    
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()

def extract_text(filepath):
    mime, _ = mimetypes.guess_type(filepath)
    ext = os.path.splitext(filepath)[1].lower()
    
    if ext == ".pdf":
        result = subprocess.run(["pdftotext", filepath, "-"], capture_output=True, text=True)
        return result.stdout
    elif mime and mime.startswith("image/"):
        result = subprocess.run(["tesseract", filepath, "stdout"], capture_output=True, text=True)
        return result.stdout
    else:
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                return f.read()
        except UnicodeDecodeError:
            show_error("Could not read text from this file type.")
            return ""

def call_ollama(prompt, context_text):
    # We will use qwen2.5-coder:3b since it's already installed on the system
    # We truncate context to ~12000 chars to avoid blowing up the context window
    data = {
        "model": "qwen2.5-coder:3b",
        "prompt": f"{prompt}\n\nDocument Content:\n{context_text[:12000]}",
        "stream": False
    }
    
    req = urllib.request.Request("http://localhost:11434/api/generate", data=json.dumps(data).encode("utf-8"), headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req) as response:
            result = json.loads(response.read().decode("utf-8"))
            return result.get("response", "")
    except Exception as e:
        show_error(f"Ollama local API error: {e}")

def analyze_file(action, filepath):
    subprocess.run(["notify-send", "-i", "applications-science", "Zenith AI", "Analyzing file locally using Ollama..."])
    
    text = extract_text(filepath)
    if not text.strip():
        show_error("No text could be extracted from the file.")
        
    if action == "extract_text":
        show_result("Extracted Text", text)
        return

    prompts = {
        "summary": "Please provide a concise but comprehensive summary of the following document.",
        "tables": "Extract any tables or structured tabular data from the following text and output it in Markdown table format.",
        "invoice": "Extract invoice information from the following text: Invoice Number, Date, Vendor Name, Total Amount, and Line Items. Format it clearly.",
        "product": "Identify the product or item described in the following text. Give the brand, model, and a brief description.",
        "tags": "Generate a comma-separated list of 10 relevant tags for the following document.",
        "keywords": "Generate 5-10 SEO-optimized keywords for the following document."
    }
    
    prompt = prompts.get(action)
    if not prompt:
        show_error(f"Unknown action: {action}")
        
    result = call_ollama(prompt, text)
    show_result(f"AI Result: {action}", result)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: zenith_ai.py <action> <filepath>")
        sys.exit(1)
    analyze_file(sys.argv[1], sys.argv[2])
