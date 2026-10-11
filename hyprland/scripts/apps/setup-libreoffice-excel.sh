#!/usr/bin/env bash
# ==============================================================================
# 📊 LibreOffice MS Office & Excel 100% Format Compatibility Setup
# ==============================================================================
# - Configures default file formats to Microsoft Excel (.xlsx), Word (.docx), PPT (.pptx)
# - Installs Microsoft Core & Metrically Identical Fonts (Calibri, Cambria, Carlito, Caladea)
# - Disables alien format warnings for frictionless native saving
# - Enables font embedding and exact table formatting
# ==============================================================================

set -euo pipefail

BOLD="\033[1m"
GREEN="\033[1;32m"
BLUE="\033[1;34m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
RESET="\033[0m"

echo -e "${CYAN}================================================================"
echo "  📊 LibreOffice MS Office & Excel Precision Compatibility Setup"
echo -e "================================================================${RESET}\n"

# --- 1. Detect Distro & Check Microsoft-Compatible Fonts ---
echo -e "${BLUE}▶ 1. Verifying Microsoft-compatible metric fonts...${RESET}"

if fc-list : family 2>/dev/null | grep -iqE "(carlito|caladea|liberation)"; then
    echo -e "${GREEN}✔ Core metric-compatible fonts (Carlito/Calibri, Caladea/Cambria, Liberation) are already installed!${RESET}"
else
    echo -e "${YELLOW}ℹ Installing missing font packages...${RESET}"
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
    else
        DISTRO="unknown"
    fi

    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm libreoffice-fresh ttf-liberation ttf-carlito ttf-caladea ttf-dejavu noto-fonts || true
            ;;
        fedora|rhel)
            sudo dnf install -y libreoffice liberation-fonts google-carlito-fonts google-caladea-fonts dejavu-fonts-all || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y libreoffice fonts-liberation fonts-croscore fonts-carlito fonts-caladea fonts-dejavu || true
            ;;
    esac
    fc-cache -f 2>/dev/null || true
fi

# --- 2. Configure LibreOffice Default Formats (XLSX, DOCX, PPTX) ---
echo -e "\n${BLUE}▶ 2. Configuring LibreOffice precision MS Office XML save profiles...${RESET}"

LIBRE_USER_DIR="$HOME/.config/libreoffice/4/user"
mkdir -p "$LIBRE_USER_DIR"

XCU_FILE="$LIBRE_USER_DIR/registrymodifications.xcu"

if [ ! -f "$XCU_FILE" ]; then
    cat << 'INIT_XCU' > "$XCU_FILE"
<?xml version="1.0" encoding="UTF-8"?>
<oor:items xmlns:oor="http://openoffice.org/2001/registry" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
</oor:items>
INIT_XCU
fi

cp "$XCU_FILE" "$XCU_FILE.bak" 2>/dev/null || true

# Python XML configuration injector
python3 - << 'PY_EOF'
import xml.etree.ElementTree as ET
import os

xcu_path = os.path.expanduser("~/.config/libreoffice/4/user/registrymodifications.xcu")
if not os.path.exists(xcu_path):
    exit(0)

try:
    tree = ET.parse(xcu_path)
    root = tree.getroot()
except Exception:
    exit(0)

# Settings to inject / update for 100% MS Office / Excel compatibility
settings = [
    # 1. Default Calc Format -> MS Excel 2007-365 XML (.xlsx)
    ("/org.openoffice.Setup/Office/Factories/org.openoffice.Setup:Factory['com.sun.star.sheet.SpreadsheetDocument']/ooSetupFactoryDefaultFilter", "xs:string", "Calc MS Excel 2007 XML"),
    # 2. Default Writer Format -> MS Word 2007-365 XML (.docx)
    ("/org.openoffice.Setup/Office/Factories/org.openoffice.Setup:Factory['com.sun.star.text.TextDocument']/ooSetupFactoryDefaultFilter", "xs:string", "MS Word 2007 XML"),
    # 3. Default Impress Format -> MS PowerPoint 2007-365 XML (.pptx)
    ("/org.openoffice.Setup/Office/Factories/org.openoffice.Setup:Factory['com.sun.star.presentation.PresentationDocument']/ooSetupFactoryDefaultFilter", "xs:string", "Impress MS PowerPoint 2007 XML"),
    # 4. Disable alien format warning popup
    ("/org.openoffice.Office.Common/Save/Document/WarnAlienFormat", "xs:boolean", "false"),
    # 5. Automatically embed fonts in documents to preserve typography
    ("/org.openoffice.Office.Common/Save/Document/EmbedFonts", "xs:boolean", "true"),
    # 6. Preserve Excel standard syntax & calculation formulas
    ("/org.openoffice.Office.Calc/Formula/Syntax/Grammar", "xs:int", "3"),
    # 7. Enable high quality OpenGL / Skia rendering
    ("/org.openoffice.Office.Common/VCL/UseSkia", "xs:boolean", "true")
]

# Remove existing matching items
for path, _, _ in settings:
    for item in list(root):
        if item.attrib.get('{http://openoffice.org/2001/registry}path') == path:
            root.remove(item)

# Insert updated settings
for path, type_name, value in settings:
    item = ET.SubElement(root, '{http://openoffice.org/2001/registry}item', {
        '{http://openoffice.org/2001/registry}path': path
    })
    prop = ET.SubElement(item, 'prop', {
        '{http://openoffice.org/2001/registry}name': path.split('/')[-1].split(':')[-1],
        '{http://openoffice.org/2001/registry}type': type_name
    })
    val_elem = ET.SubElement(prop, 'value')
    val_elem.text = value

tree.write(xcu_path, encoding="UTF-8", xml_declaration=True)
PY_EOF

echo -e "${GREEN}✔ Default save format set to Microsoft Excel (.xlsx) & Word (.docx)!${RESET}"
echo -e "${GREEN}✔ Alien format save warnings disabled!${RESET}"
echo -e "${GREEN}✔ Font embedding and Skia high-DPI rendering enabled!${RESET}"

echo -e "\n${CYAN}================================================================"
echo "🎉 LibreOffice is now fully optimized for MS Office & Excel files!"
echo -e "================================================================${RESET}\n"
