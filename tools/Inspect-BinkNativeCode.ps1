# Static, read-only inspection of the supported Spider-Man 2001 PC executable.
# Windows PowerShell 5.1+; no Python, debugger, administrator, or game launch.
# Usage (from game directory):
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\Inspect-BinkNativeCode.ps1
# Produces only hex/code addresses in a text report. Does NOT modify the EXE.
# The tool does NOT disassemble instructions and does NOT identify scene owners.
[CmdletBinding()]
param(
    [string]$GameExe = ".\SpideyPC.exe",
    [string]$Report = ".\SpiderMan2k1Fix-BinkNativeCode.txt"
)

$ErrorActionPreference = "Stop"
$expectedHash = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
$expectedLength = 1507328
$targetBinkOwner = [long]0x00470750
$lines = New-Object 'System.Collections.Generic.List[string]'

function Ensure-Span {
    param([long]$Offset,[long]$Length,[long]$Size)
    if($Offset -lt 0 -or $Length -lt 0 -or $Offset -gt $Size -or $Length -gt ($Size-$Offset)) {
        throw ("Invalid PE file bounds: offset={0}, size={1}, file={2}" -f $Offset,$Length,$Size)
    }
}

function Resolve-Address {
    param([long]$Address,[long]$Length)
    $rva = $Address - $imageBase
    if($rva -lt 0) { throw ("Address 0x{0:X8} below image base" -f $Address) }
    foreach($section in $sections) {
        if($rva -ge $section.RVA -and
           $rva -lt ($section.RVA + $section.RawSize) -and
           $Length -le ($section.RVA + $section.RawSize - $rva)) {
            $fileOffset = $section.RawPointer + ($rva-$section.RVA)
            Ensure-Span $fileOffset $Length $bytes.Length
            return [long]$fileOffset
        }
    }
    throw ("Address range 0x{0:X8}+{1} absent from file-backed PE sections" -f $Address,$Length)
}

function Emit-HexRegion {
    param([string]$Label,[long]$Start,[int]$Length)
    if($Length -gt 2048 -or $Length -le 0) { throw "Unsupported code-window size" }
    $first = Resolve-Address $Start $Length
    $lines.Add("")
    $lines.Add(("[{0}] VA=0x{1:X8} bytes={2}" -f $Label,$Start,$Length))
    for($pos=0;$pos -lt $Length;$pos+=16) {
        $count = [Math]::Min(16,$Length-$pos)
        $slice = New-Object string[] $count
        for($j=0;$j -lt $count;$j++) {
            $slice[$j] = $bytes[[int]($first+$pos+$j)].ToString("X2")
        }
        $lines.Add(("0x{0:X8}: {1}" -f ($Start+$pos),($slice -join " ")))
    }
}

$exe = (Resolve-Path -LiteralPath $GameExe -ErrorAction Stop).ProviderPath
if([IO.Path]::GetFileName($exe) -ine "SpideyPC.exe") {
    throw "This inspection supports only the original SpideyPC.exe"
}
$bytes = [IO.File]::ReadAllBytes($exe)
if($bytes.Length -ne $expectedLength) {
    throw ("Unsupported EXE length {0}, expected {1}" -f $bytes.Length,$expectedLength)
}
$hasher = [Security.Cryptography.SHA256]::Create()
try {
    $hash = ([BitConverter]::ToString($hasher.ComputeHash($bytes))).Replace("-","").ToLowerInvariant()
} finally { $hasher.Dispose() }
if($hash -ne $expectedHash) { throw "Unsupported EXE SHA-256. No report produced." }
Ensure-Span 0 64 $bytes.Length
if($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { throw "Invalid DOS/PE header" }
$peOffset = [long][BitConverter]::ToUInt32($bytes,0x3C)
Ensure-Span $peOffset 24 $bytes.Length
if($bytes[[int]$peOffset] -ne 0x50 -or
   $bytes[[int]($peOffset+1)] -ne 0x45 -or
   $bytes[[int]($peOffset+2)] -ne 0 -or
   $bytes[[int]($peOffset+3)] -ne 0) { throw "Invalid PE signature" }
$sectionCount = [int][BitConverter]::ToUInt16($bytes,[int]($peOffset+6))
$optionalSize = [int][BitConverter]::ToUInt16($bytes,[int]($peOffset+20))
$optionalOffset = $peOffset+24
Ensure-Span $optionalOffset $optionalSize $bytes.Length
if($optionalSize -lt 32 -or [BitConverter]::ToUInt16($bytes,[int]$optionalOffset) -ne 0x10B) {
    throw "Expected a 32-bit PE executable"
}
$imageBase = [long][BitConverter]::ToUInt32($bytes,[int]($optionalOffset+28))
if($imageBase -ne 0x00400000) { throw "Unexpected PE image base" }
if($sectionCount -lt 1 -or $sectionCount -gt 48) { throw "Invalid section count" }
$tableOffset = $optionalOffset+$optionalSize
Ensure-Span $tableOffset ($sectionCount*40) $bytes.Length
$sections = @()
for($i=0;$i -lt $sectionCount;$i++) {
    $at = [int]($tableOffset+$i*40)
    $secName = [Text.Encoding]::ASCII.GetString($bytes,$at,8).Trim([char]0)
    $virtualRVA = [long][BitConverter]::ToUInt32($bytes,$at+12)
    $rawSize = [long][BitConverter]::ToUInt32($bytes,$at+16)
    $rawPointer = [long][BitConverter]::ToUInt32($bytes,$at+20)
    $flags = [long][BitConverter]::ToUInt32($bytes,$at+36)
    if($rawSize -gt 0) { Ensure-Span $rawPointer $rawSize $bytes.Length }
    $sections += [pscustomobject]@{
        Name=$secName; RVA=$virtualRVA; RawSize=$rawSize
        RawPointer=$rawPointer; Characteristics=$flags
    }
}
$lines.Add("SpiderMan2k1Fix static Bink-owner inspection (NO game execution)")
$lines.Add(("EXE SHA256: {0}; size: {1}; image base: 0x{2:X8}" -f $hash,$bytes.Length,$imageBase))
$lines.Add("Hex rows are instruction BYTES, not disassembly or a proven input branch.")
$lines.Add("Do not publish raw game code in the public GitHub repository.")
Emit-HexRegion "Bink loop and close-path candidates" 0x00470700 0x350
Emit-HexRegion "Native top-level transition" 0x00455C80 0x200

# Discover raw near CALL (E8 rel32) candidates to 0x470750 in executable
# sections. A byte pattern is NOT proof of an instruction boundary.
$callers = New-Object 'System.Collections.Generic.List[long]'
foreach($section in $sections) {
    if(($section.Characteristics -band [long]0x20000000) -eq 0) { continue }
    $baseOffset = [long]$section.RawPointer
    for([long]$i=0;$i -le ($section.RawSize-5);$i++) {
        if($bytes[[int]($baseOffset+$i)] -ne 0xE8) { continue }
        $relative = [long][BitConverter]::ToInt32($bytes,[int]($baseOffset+$i+1))
        $callVA = $imageBase+$section.RVA+$i
        if($callVA+5+$relative -eq $targetBinkOwner) {
            $callers.Add($callVA)
            if($callers.Count -ge 24) { break }
        }
    }
    if($callers.Count -ge 24) { break }
}
$lines.Add("")
$lines.Add(("[Raw E8-rel32 call candidates to 0x{0:X8}] count={1}" -f $targetBinkOwner,$callers.Count))
$lines.Add("Each candidate needs instruction-boundary verification before use.")
foreach($caller in $callers) {
    $lines.Add(("Candidate CALL at 0x{0:X8}" -f $caller))
    # Caller context includes 48 bytes before and 80 bytes after.
    try { Emit-HexRegion ("Call-site neighborhood 0x{0:X8}" -f $caller) ($caller-48) 0x80 }
    catch { $lines.Add(("Context unavailable at 0x{0:X8}: {1}" -f $caller,$_.Exception.Message)) }
}
$destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Report)
[IO.File]::WriteAllLines($destination,$lines,[Text.Encoding]::UTF8)
Write-Host ("Read-only static inspection complete: {0}" -f $destination)
Write-Host ("Direct-call byte-pattern candidates: {0}" -f $callers.Count)
Write-Host "Upload only this text report; keep the original EXE private."
