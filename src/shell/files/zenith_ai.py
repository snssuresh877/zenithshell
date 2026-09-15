#!/usr/bin/env python3
import sys, os, subprocess, json

def show_error(msg):
    subprocess.run(["notify-send", "-u", "critical", "Zenith AI Error", msg])
    sys.exit(1)

try:
    from google import genai
    from google.genai import types
except ImportError:
    show_error("Please install the google-genai python package: pip install google-genai")

def get_api_key():
    key_path = os.path.expanduser("~/.config/zenithshell/gemini_key.txt")
    if os.path.exists(key_path):
        with open(key_path, "r") as f:
            return f.read().strip()
    
    # Prompt user for key
    try:
        result = subprocess.run(
            ["zenity", "--entry", "--title=Zenith AI Setup", "--text=Enter your Google Gemini API Key:"],
            capture_output=True, text=True
        )
        if result.returncode == 0 and result.stdout.strip():
            key = result.stdout.strip()
            os.makedirs(os.path.dirname(key_path), exist_ok=True)
            with open(key_path, "w") as f:
                f.write(key)
            return key
    except FileNotFoundError:
        show_error("Zenity is required for UI prompts.")
    return None

def show_result(title, content):
    # Use zenity text info for a scrollable window
    process = subprocess.Popen(
        ["zenity", "--text-info", f"--title={title}", "--width=600", "--height=500"],
        stdin=subprocess.PIPE
    )
    process.communicate(input=content.encode('utf-8'))

def analyze_file(action, filepath):
    key = get_api_key()
    if not key:
        show_error("API key is required.")
    
    client = genai.Client(api_key=key)
    
    prompts = {
        "extract_text": "Extract all text from this file verbatim.",
        "summary": "Provide a concise but comprehensive summary of this document.",
        "tables": "Extract all tables or structured tabular data from this file and output it in Markdown table format.",
        "invoice": "Extract all invoice information: Invoice Number, Date, Vendor Name, Total Amount, and Line Items. Output as JSON or structured Markdown.",
        "product": "Identify the product in this image. Give the brand, model, and a brief description.",
        "tags": "Generate a comma-separated list of 10 relevant tags for this file.",
        "keywords": "Generate SEO-optimized keywords for this file."
    }
    
    prompt = prompts.get(action)
    if not prompt:
        show_error(f"Unknown action: {action}")
        
    subprocess.run(["notify-send", "-i", "applications-science", "Zenith AI", "Analyzing file..."])
    
    try:
        # Upload file using the Files API (supports images, PDFs, text)
        uploaded_file = client.files.upload(file=filepath)
        
        # We will use gemini-2.5-flash as it supports multimodal and is fast
        response = client.models.generate_content(
            model='gemini-2.5-flash',
            contents=[uploaded_file, prompt]
        )
        
        show_result(f"AI Result: {action}", response.text)
        
    except Exception as e:
        show_error(str(e))

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: zenith_ai.py <action> <filepath>")
        sys.exit(1)
    analyze_file(sys.argv[1], sys.argv[2])
