# TrekLink Migration Script: Proprietary → Meshtastic v2.6.x Fork
# Generated: 2026-02-04
# Purpose: Archive proprietary E32 UART code and prepare Meshtastic fork workspace

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TrekLink → Meshtastic Migration Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"
$originalRepo = "d:\DATA\Github\treklink"
$meshtasticRepo = "d:\DATA\Github\meshtastic-treklink"
$backupDate = Get-Date -Format "yyyy-MM-dd_HHmmss"

# ============================================================================
# STEP 1: Archive Current Proprietary Code
# ============================================================================

Write-Host "[STEP 1] Archiving proprietary code..." -ForegroundColor Yellow

# Navigate to original repo
Set-Location $originalRepo

# Check if git repo exists
if (!(Test-Path ".git")) {
    Write-Host "ERROR: Not a git repository. Initializing git..." -ForegroundColor Red
    git init
    git add -A
    git commit -m "Initial commit: Proprietary E32 UART implementation"
}

# Create backup branch
Write-Host "Creating archive branch: archive/proprietary-v1.0" -ForegroundColor Green
try {
    # Check current branch
    $currentBranch = git rev-parse --abbrev-ref HEAD
    Write-Host "Current branch: $currentBranch" -ForegroundColor Gray
    
    # Stash any uncommitted changes
    git stash push -m "Auto-stash before migration"
    
    # Create and checkout archive branch
    git checkout -b "archive/proprietary-v1.0" 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Branch already exists, switching to it..." -ForegroundColor Yellow
        git checkout "archive/proprietary-v1.0"
    }
    
    # Commit all current state
    git add -A
    git commit -m "Archive: Proprietary E32 UART implementation before Meshtastic migration (${backupDate})" --allow-empty
    
    Write-Host "✓ Proprietary code archived in branch 'archive/proprietary-v1.0'" -ForegroundColor Green
} catch {
    Write-Host "WARNING: Git archiving failed: $_" -ForegroundColor Yellow
    Write-Host "Continuing with ZIP backup..." -ForegroundColor Yellow
}

# Create ZIP backup (safety net)
Write-Host "Creating ZIP backup..." -ForegroundColor Green
$backupFiles = @("src", "include", "lib", "test", "platformio.ini", ".kiro")
$backupPath = Join-Path $originalRepo "treklink_proprietary_backup_${backupDate}.zip"

$filesToCompress = @()
foreach ($item in $backupFiles) {
    $fullPath = Join-Path $originalRepo $item
    if (Test-Path $fullPath) {
        $filesToCompress += $fullPath
    }
}

if ($filesToCompress.Count -gt 0) {
    Compress-Archive -Path $filesToCompress -DestinationPath $backupPath -Force
    Write-Host "✓ ZIP backup created: $backupPath" -ForegroundColor Green
} else {
    Write-Host "WARNING: No files found to backup!" -ForegroundColor Yellow
}

# ============================================================================
# STEP 2: Clone Fresh Meshtastic Firmware Repository
# ============================================================================

Write-Host ""
Write-Host "[STEP 2] Cloning Meshtastic firmware v2.6.x..." -ForegroundColor Yellow

# Check if destination already exists
if (Test-Path $meshtasticRepo) {
    Write-Host "WARNING: Directory already exists: $meshtasticRepo" -ForegroundColor Yellow
    $response = Read-Host "Delete and re-clone? (y/N)"
    if ($response -eq "y" -or $response -eq "Y") {
        Remove-Item -Recurse -Force $meshtasticRepo
        Write-Host "✓ Removed existing directory" -ForegroundColor Green
    } else {
        Write-Host "Skipping clone, using existing directory..." -ForegroundColor Yellow
        Set-Location $meshtasticRepo
        git fetch origin
        git checkout v2.6.x
        git pull origin v2.6.x
        Write-Host "✓ Updated existing Meshtastic repo to latest v2.6.x" -ForegroundColor Green
        $skipClone = $true
    }
}

if (-not $skipClone) {
    # Clone Meshtastic firmware
    Write-Host "Cloning from https://github.com/meshtastic/firmware.git..." -ForegroundColor Green
    Set-Location "d:\DATA\Github"
    
    git clone --branch v2.6.x --depth 1 https://github.com/meshtastic/firmware.git meshtastic-treklink
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Meshtastic firmware cloned successfully" -ForegroundColor Green
    } else {
        Write-Host "ERROR: Git clone failed!" -ForegroundColor Red
        exit 1
    }
    
    Set-Location $meshtasticRepo
}

# Verify clone
$meshtasticVersion = git describe --tags
Write-Host "Meshtastic version: $meshtasticVersion" -ForegroundColor Cyan

# ============================================================================
# STEP 3: Create TrekLink Custom Variant Directory (Phase 1 Task 1.2)
# ============================================================================

Write-Host ""
Write-Host "[STEP 3] Creating TrekLink custom variant..." -ForegroundColor Yellow

$variantDir = Join-Path $meshtasticRepo "variants\treklink_esp32"

# Create variant directory
if (!(Test-Path $variantDir)) {
    New-Item -ItemType Directory -Path $variantDir -Force | Out-Null
    Write-Host "✓ Created directory: variants\treklink_esp32" -ForegroundColor Green
} else {
    Write-Host "Directory already exists: variants\treklink_esp32" -ForegroundColor Yellow
}

# Find a suitable reference variant to copy
$referenceVariants = @(
    "variants\diy\v1\variant.h",
    "variants\heltec_v1\variant.h",
    "variants\tlora_v1\variant.h"
)

$referenceVariant = $null
foreach ($variant in $referenceVariants) {
    $fullPath = Join-Path $meshtasticRepo $variant
    if (Test-Path $fullPath) {
        $referenceVariant = $fullPath
        Write-Host "Found reference variant: $variant" -ForegroundColor Green
        break
    }
}

if ($referenceVariant) {
    # Copy reference variant as starting point
    $targetVariantFile = Join-Path $variantDir "variant.h"
    Copy-Item $referenceVariant $targetVariantFile -Force
    Write-Host "✓ Copied reference variant.h to TrekLink variant directory" -ForegroundColor Green
    
    # Add comment header to variant.h
    $headerComment = @"
/*
 * TrekLink ESP32 Custom Variant
 * Hardware: ESP32-WROOM-32 + Ra-02 SX1278 433MHz SPI
 * Version: 2.0 (Meshtastic Fork)
 * Date: $(Get-Date -Format "yyyy-MM-dd")
 * 
 * GPIO Configuration:
 * - LoRa Ra-02 (SPI): SCK=5, MISO=19, MOSI=27, CS=18, DIO0=26, RESET=14
 * - I2C Bus: SDA=21, SCL=22 (OLED 0x3C + MPU6050 0x68)
 * - GPS Neo-6M (UART1): TX=16, RX=17
 * - Buttons: MENU=25, SOS=34, UP=32, DOWN=35
 * - Buzzer=33, Vibrator=4, LED=2
 * - Power Gates: GPS_PWR=13, OLED_GND=23
 * - Battery ADC: GPIO 36
 */

"@
    
    $existingContent = Get-Content $targetVariantFile -Raw
    $headerComment + $existingContent | Set-Content $targetVariantFile -NoNewline
    
    Write-Host "✓ Added TrekLink header comment to variant.h" -ForegroundColor Green
} else {
    Write-Host "WARNING: No reference variant found. Creating minimal variant.h..." -ForegroundColor Yellow
    
    # Create minimal variant.h from scratch
    $minimalVariant = @"
/*
 * TrekLink ESP32 Custom Variant
 * Hardware: ESP32-WROOM-32 + Ra-02 SX1278 433MHz SPI
 */

#ifndef _VARIANT_TREKLINK_ESP32_
#define _VARIANT_TREKLINK_ESP32_

// LoRa Ra-02 SX1278 (SPI)
#define LORA_SCK    5
#define LORA_MISO   19
#define LORA_MOSI   27
#define LORA_CS     18
#define LORA_DIO0   26
#define LORA_RESET  14

// Region configuration
#define LORA_REGION Meshtastic_Region_EU_433

// I2C Bus (OLED + MPU6050)
#define I2C_SDA     21
#define I2C_SCL     22

// GPS Neo-6M (UART1)
#define GPS_RX_PIN  16  // ESP32 TX1
#define GPS_TX_PIN  17  // ESP32 RX1

// Buttons
#define BUTTON_PIN  25  // MENU (Meshtastic default)
// Phase 2: BTN_SOS=34, BTN_UP=32, BTN_DOWN=35

// Notifications
#define PIN_BUZZER    33
#define PIN_VIBRATOR  4
#define LED_PIN       2

// Power Management
#define PIN_GPS_PWR_EN    13
#define PIN_OLED_GND_EN   23
#define BATTERY_PIN       36  // ADC1_CH0

#endif // _VARIANT_TREKLINK_ESP32_
"@
    
    $targetVariantFile = Join-Path $variantDir "variant.h"
    $minimalVariant | Set-Content $targetVariantFile -NoNewline
    Write-Host "✓ Created minimal variant.h" -ForegroundColor Green
}

# ============================================================================
# STEP 4: Copy Specification Documents
# ============================================================================

Write-Host ""
Write-Host "[STEP 4] Copying specification documents..." -ForegroundColor Yellow

# Create docs directory in Meshtastic repo
$docsDir = Join-Path $meshtasticRepo "docs\treklink"
if (!(Test-Path $docsDir)) {
    New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
}

# Copy specification files
$specFiles = @(
    @{Source = "$originalRepo\specifications.md"; Dest = "$docsDir\specifications.md"},
    @{Source = "$originalRepo\.kiro\specs\treklink\requirements.md"; Dest = "$docsDir\requirements.md"},
    @{Source = "$originalRepo\.kiro\specs\treklink\design.md"; Dest = "$docsDir\design.md"},
    @{Source = "$originalRepo\.kiro\specs\treklink\tasks.md"; Dest = "$docsDir\tasks.md"}
)

foreach ($file in $specFiles) {
    if (Test-Path $file.Source) {
        Copy-Item $file.Source $file.Dest -Force
        Write-Host "✓ Copied $(Split-Path $file.Source -Leaf)" -ForegroundColor Green
    } else {
        Write-Host "WARNING: File not found: $($file.Source)" -ForegroundColor Yellow
    }
}

# Copy implementation plan from artifacts
$artifactPlan = "C:\Users\PC\.gemini\antigravity\brain\8b95c2df-5ee8-4678-8d5d-b41e629acc51\implementation_plan.md"
if (Test-Path $artifactPlan) {
    Copy-Item $artifactPlan "$docsDir\implementation_plan.md" -Force
    Write-Host "✓ Copied implementation_plan.md from artifacts" -ForegroundColor Green
}

# ============================================================================
# STEP 5: Summary and Next Steps
# ============================================================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Migration Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "COMPLETED STEPS:" -ForegroundColor Yellow
Write-Host "  ✓ Proprietary code archived to: archive/proprietary-v1.0 branch" -ForegroundColor Green
Write-Host "  ✓ ZIP backup created: treklink_proprietary_backup_${backupDate}.zip" -ForegroundColor Green
Write-Host "  ✓ Meshtastic v2.6.x cloned to: $meshtasticRepo" -ForegroundColor Green
Write-Host "  ✓ TrekLink variant created: variants\treklink_esp32\variant.h" -ForegroundColor Green
Write-Host "  ✓ Specification docs copied to: docs\treklink\" -ForegroundColor Green
Write-Host ""

Write-Host "NEXT STEPS (Phase 1 Tasks):" -ForegroundColor Yellow
Write-Host "  1. Open VSCode in: $meshtasticRepo" -ForegroundColor White
Write-Host "  2. Edit variants\treklink_esp32\variant.h (configure GPIO pins)" -ForegroundColor White
Write-Host "  3. Edit platformio.ini (add [env:treklink-esp32] environment)" -ForegroundColor White
Write-Host "  4. Build firmware: pio run -e treklink-esp32" -ForegroundColor White
Write-Host "  5. Flash to ESP32: pio run -e treklink-esp32 -t upload" -ForegroundColor White
Write-Host ""

Write-Host "REFERENCE DOCUMENTATION:" -ForegroundColor Yellow
Write-Host "  - Tasks:          $docsDir\tasks.md" -ForegroundColor Cyan
Write-Host "  - Design:         $docsDir\design.md" -ForegroundColor Cyan
Write-Host "  - Requirements:   $docsDir\requirements.md" -ForegroundColor Cyan
Write-Host "  - Implementation: $docsDir\implementation_plan.md" -ForegroundColor Cyan
Write-Host ""

Write-Host "PROPRIETARY CODE (for Phase 2 reference):" -ForegroundColor Yellow
Write-Host "  - Git Branch:     archive/proprietary-v1.0" -ForegroundColor Cyan
Write-Host "  - ZIP Backup:     $backupPath" -ForegroundColor Cyan
Write-Host ""

Write-Host "Ready to start Phase 1 implementation!" -ForegroundColor Green
Write-Host ""

# Open VSCode in Meshtastic repo (optional)
$openVSCode = Read-Host "Open VSCode in Meshtastic repo? (Y/n)"
if ($openVSCode -ne "n" -and $openVSCode -ne "N") {
    Write-Host "Opening VSCode..." -ForegroundColor Green
    code $meshtasticRepo
}
