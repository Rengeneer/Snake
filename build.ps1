# ============================================
# Snake GPU Bot - Automated Build Script
# For Windows with AMD HIP support
# ============================================

param(
    [string]$BuildType = "Release",
    [string]$HipPath = "C:\Program Files\AMD\ROCm\5.7",
    [switch]$Clean,
    [switch]$Test,
    [switch]$Run,
    [switch]$NoBuild,
    [switch]$NoGPU
)

$ErrorActionPreference = "Stop"

# Colors
$ColorInfo = "Cyan"
$ColorSuccess = "Green"
$ColorWarning = "Yellow"
$ColorError = "Red"

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-Host "============================================" -ForegroundColor $ColorInfo
    Write-Host " $Message" -ForegroundColor $ColorInfo
    Write-Host "============================================" -ForegroundColor $ColorInfo
    Write-Host ""
}

function Write-Status {
    param([string]$Message)
    Write-Host "▶ $Message" -ForegroundColor $ColorInfo
}

function Write-Success {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor $ColorSuccess
}

function Write-Warning {
    param([string]$Message)
    Write-Host "⚠ $Message" -ForegroundColor $ColorWarning
}

function Write-Error-Message {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor $ColorError
}

# ============================================
# Main Script
# ============================================

Write-Header "Snake GPU Bot - Build Script"

# Check prerequisites
Write-Status "Checking prerequisites..."

# Check CMake
try {
    $cmakeVersion = cmake --version 2>&1 | Select-Object -First 1
    Write-Success "CMake found: $cmakeVersion"
} catch {
    Write-Error-Message "CMake not found! Please install CMake."
    exit 1
}

# Check Visual Studio
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community"
if (Test-Path $vsPath) {
    Write-Success "Visual Studio 2022 found"
} else {
    $vsPath = "C:\Program Files\Microsoft Visual Studio\2019\Community"
    if (Test-Path $vsPath) {
        Write-Success "Visual Studio 2019 found"
    } else {
        Write-Error-Message "Visual Studio not found!"
        exit 1
    }
}

# Check HIP (optional)
if (-not $NoGPU) {
    if (Test-Path $HipPath) {
        Write-Success "HIP found at: $HipPath"
        $useGpu = $true
    } else {
        Write-Warning "HIP not found at: $HipPath"
        Write-Warning "Building without GPU support (CPU bot only)"
        $useGpu = $false
    }
} else {
    Write-Warning "GPU support disabled (--NoGPU flag)"
    $useGpu = $false
}

# Clean build directory if requested
if ($Clean) {
    Write-Status "Cleaning build directory..."
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
        Write-Success "Build directory cleaned"
    }
}

# Create build directory
if (-not (Test-Path "build")) {
    Write-Status "Creating build directory..."
    New-Item -ItemType Directory -Path "build" | Out-Null
    Write-Success "Build directory created"
}

# Configure CMake
if (-not $NoBuild) {
    Write-Header "Configuring CMake"
    
    Set-Location "build"
    
    try {
        if ($useGpu) {
            Write-Status "Configuring with GPU support..."
            cmake .. -G "Visual Studio 17 2022" -A x64 `
                -DCMAKE_BUILD_TYPE=$BuildType `
                -DHIP_PATH=$HipPath
        } else {
            Write-Status "Configuring without GPU support..."
            # Temporarily rename gpu directory to disable GPU build
            if (Test-Path "..\src\gpu") {
                Rename-Item "..\src\gpu" "..\src\gpu_disabled"
            }
            
            cmake .. -G "Visual Studio 17 2022" -A x64 `
                -DCMAKE_BUILD_TYPE=$BuildType
            
            # Restore gpu directory
            if (Test-Path "..\src\gpu_disabled") {
                Rename-Item "..\src\gpu_disabled" "..\src\gpu"
            }
        }
        
        Write-Success "CMake configuration complete"
    } catch {
        Write-Error-Message "CMake configuration failed: $_"
        Set-Location ..
        exit 1
    }
    
    # Build project
    Write-Header "Building Project"
    
    Write-Status "Building $BuildType configuration..."
    try {
        cmake --build . --config $BuildType -- /m /verbosity:minimal
        Write-Success "Build complete!"
    } catch {
        Write-Error-Message "Build failed: $_"
        Set-Location ..
        exit 1
    }
    
    Set-Location ..
}

# Run tests if requested
if ($Test) {
    Write-Header "Running Tests"
    
    if (Test-Path "bin\Snake_gpu_test.exe") {
        Write-Status "Running GPU tests..."
        & "bin\Snake_gpu_test.exe"
    } else {
        Write-Warning "GPU test executable not found"
    }
}

# Run game if requested
if ($Run) {
    Write-Header "Launching Game"
    
    if (Test-Path "bin\Snake.exe") {
        Write-Status "Starting Snake..."
        Start-Process "bin\Snake.exe"
        Write-Success "Game launched!"
    } else {
        Write-Error-Message "Snake.exe not found in bin directory"
        exit 1
    }
}

# Summary
Write-Header "Build Summary"

Write-Host "Build Type:    $BuildType" -ForegroundColor $ColorInfo
Write-Host "GPU Support:   $(if ($useGpu) { 'Enabled' } else { 'Disabled' })" -ForegroundColor $(if ($useGpu) { $ColorSuccess } else { $ColorWarning })

if (Test-Path "bin\Snake.exe") {
    $exeSize = (Get-Item "bin\Snake.exe").Length / 1MB
    Write-Host "Executable:    bin\Snake.exe ($([math]::Round($exeSize, 2)) MB)" -ForegroundColor $ColorSuccess
}

if (Test-Path "bin\Snake_gpu_test.exe") {
    Write-Host "GPU Test:      bin\Snake_gpu_test.exe" -ForegroundColor $ColorSuccess
}

Write-Host ""
Write-Success "All done!"
Write-Host ""
Write-Host "Quick commands:" -ForegroundColor $ColorInfo
Write-Host "  Run game:     .\bin\Snake.exe" -ForegroundColor Gray
Write-Host "  Run tests:    .\bin\Snake_gpu_test.exe" -ForegroundColor Gray
Write-Host "  Benchmark:    .\bin\Snake_bench.exe" -ForegroundColor Gray
Write-Host ""
Write-Host "Script usage:" -ForegroundColor $ColorInfo
Write-Host "  Build:        .\build.ps1" -ForegroundColor Gray
Write-Host "  Clean build:  .\build.ps1 -Clean" -ForegroundColor Gray
Write-Host "  Build & run:  .\build.ps1 -Run" -ForegroundColor Gray
Write-Host "  Build & test: .\build.ps1 -Test" -ForegroundColor Gray
Write-Host "  CPU only:     .\build.ps1 -NoGPU" -ForegroundColor Gray
Write-Host "  Custom HIP:   .\build.ps1 -HipPath 'C:\custom\path'" -ForegroundColor Gray
Write-Host ""


