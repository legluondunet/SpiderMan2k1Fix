"""Standalone recovery parser and integration safety checks."""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.fixture(scope='module')
def recover_runner(tmp_path_factory):
    if not shutil.which('gcc'):
        pytest.skip('requires gcc')
    output=tmp_path_factory.mktemp('ini-recover')/'runner'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror',
                    str(ROOT/'ChatGPT/tests/test_ini_recover_runner.c'),'-o',str(output)],
                   capture_output=True,text=True,check=True)
    return output

def recover(runner,tmp_path,data):
    cfg=tmp_path/'SpiderMan2k1Fix.ini'
    cfg.write_bytes(data.encode() if isinstance(data,str) else data)
    r=subprocess.run([str(runner),str(cfg)],capture_output=True,text=True,check=True)
    lines=r.stdout.splitlines()
    return lines[1:]

def test_valid_ini_preserves_known(recover_runner,tmp_path):
    values=recover(recover_runner,tmp_path,(ROOT/'ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini').read_text())
    assert '[Audio] MusicSFXLevel=69' in values
    assert '[Keyboard] Jump=SPACE' in values
    assert '[Audio] Stereo=1' in values
    assert len(next(v.split('=',1)[1] for v in values if v.startswith('[Internal] RawHex=')))==504

@pytest.mark.parametrize('data,wanted,not_wanted',[
    ('[Audio]\nVoiceLevel=bad\nVoiceLevel=79\nVoiceLevel=30\n','[Audio] VoiceLevel=79','[Audio] VoiceLevel=30'),
    ('[Audio]\nVoiceLevel=12\nVoiceLevel=79\n','[Audio] VoiceLevel=12','[Audio] VoiceLevel=79'),
    ('[Audio]\nVoiceLevel=10\n[Audio]\nVoiceLevel=90\n','[Audio] VoiceLevel=10','[Audio] VoiceLevel=90'),
    ('[Audio]\nVoiceLevel=bad\n[Audio]\nVoiceLevel=90\n','[Audio] VoiceLevel=90','[Audio] VoiceLevel=bad'),
    ('[Controls]\nJump=F\n[Keyboard]\nJump=SPACE\n','[Keyboard] Jump=SPACE','[Controls] Jump=F'),
    ('[Unknown]\nStereo=0\n[Audio]\nStereo=1\n','[Audio] Stereo=1','[Unknown] Stereo=0'),
    ('[Display]\nResolution=garbage\nResolution=Auto\n','[Display] Resolution=Auto','[Display] Resolution=garbage'),
    ('[Keyboard]\nJump=SPCAE\nJump=0x21\n','[Keyboard] Jump=0x21','[Keyboard] Jump=SPCAE'),
    ('[Joystick]\nJump=BUTTON300\nJump=BUTTON4\n','[Joystick] Jump=BUTTON4','[Joystick] Jump=BUTTON300'),
    ('[GameConfig]\nRawHex=abc\n[Internal]\nRawHex='+'12'*252+'\n','[Internal] RawHex='+'12'*252,'[GameConfig] RawHex='),
])
def test_first_valid_recovers(recover_runner,tmp_path,data,wanted,not_wanted):
    values=recover(recover_runner,tmp_path,data)
    assert wanted in values
    assert not_wanted not in values

def test_invalid_utf16_not_salvaged(recover_runner,tmp_path):
    assert recover(recover_runner,tmp_path,b'\xff\xfe[\x00A\x00u\x00d\x00i\x00o\x00]')==[]

def test_binary_null_does_not_replace_good_values(recover_runner,tmp_path):
    values=recover(recover_runner,tmp_path,b'[Audio]\nVoiceLevel=79\nVoiceLevel=0\x00oops\n')
    assert values==['[Audio] VoiceLevel=79']

def test_section_after_malformed_header(recover_runner,tmp_path):
    values=recover(recover_runner,tmp_path,'[Audio]\nVoiceLevel=50\n[Broken\nVoiceLevel=13\n[Keyboard]\nJump=F\n')
    assert values==['[Audio] VoiceLevel=50','[Keyboard] Jump=F']

def test_unknown_lines_not_recovered(recover_runner,tmp_path):
    values=recover(recover_runner,tmp_path,'garbage\n[Plugin]\nFoo=Bar\n[Audio]\nMovieLevel=100\nWut=1\n')
    assert values==['[Audio] MovieLevel=100']

def test_repair_wiring_is_safe_and_atomic():
    src=(ROOT/'virtual_spidey_cfg.c').read_text()
    assert 'SpiderCfg_WriteDefaultIniAt(tmp,0,logger)' in src
    assert 'SpiderIni_Recover(ini,values)' in src
    assert 'MoveFileExA(tmp,ini,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)' in src
    assert src.index('CopyFileA(ini,backup,TRUE)') < src.index('MoveFileExA(tmp,ini,')
    assert src.index('SpiderIni_Validate(tmp,NULL,NULL)') < src.index('CopyFileA(ini,backup,TRUE)')
    assert 'if(!ini_writes_permitted)' in src
    assert 'SpiderCfg_ValidateCurrentIni(log_line)' in (ROOT/'spiderman2k1fix.c').read_text()


def test_repair_preserves_f12_auto3d_opt_in_and_other_valid_settings(recover_runner, tmp_path):
    """Recover first valid debug opt-in instead of losing it on repair."""
    ini = (
        "[Debug]\nLog=1\nF12TestMode=1\n"
        "F12Auto3DScripts=1\n"
        "[Display]\nResolution=1920x1080\n"
        "[Keyboard]\nJump=SPACE\n"
        "[Unknown]\nKey=bad\n"
    )
    values = recover(recover_runner, tmp_path, ini)
    assert "[Debug] Log=1" in values
    assert "[Debug] F12TestMode=1" in values
    assert "[Debug] F12Auto3DScripts=1" in values
    assert "[Display] Resolution=1920x1080" in values
    assert "[Keyboard] Jump=SPACE" in values
    assert not any("Unknown" in item for item in values)


def test_repair_excludes_invalid_f12_auto3d_values(recover_runner, tmp_path):
    values = recover(recover_runner, tmp_path,
                     "[Debug]\nF12Auto3DScripts=INVALID\nF12Auto3DScripts=1\n")
    assert "[Debug] F12Auto3DScripts=1" in values
    assert "[Debug] F12Auto3DScripts=INVALID" not in values


def test_canonical_f10_and_legacy_f12_config_retained_through_recovery(recover_runner,tmp_path):
    user_ini=("[Debug]\nLog=1\nF10TestMode=1\nF10Auto3DScripts=1\n"
       "F12TestMode=0\nF12Auto3DScripts=0\n[System]\n"
       "QuickExitF10=1\nQuickExitF12=0\n[Audio]\nVoiceLevel=71\n")
    lines=recover(recover_runner,tmp_path,user_ini)
    for entry in ("[Debug] F10TestMode=1","[Debug] F10Auto3DScripts=1",
                  "[Debug] F12TestMode=0","[System] QuickExitF10=1",
                  "[System] QuickExitF12=0","[Audio] VoiceLevel=71"):
        assert entry in lines


def test_auto_repair_preserves_f10_gameplay_exit(recover_runner,tmp_path):
    lines=recover(recover_runner,tmp_path,
        "[Debug]\nLog=1\nF10TestMode=1\nF10Auto3DScripts=1\nF10GameplayExit=1\n"
        "[Keyboard]\nJump=SPACE\n[Plugin]\nUnknown=x\n")
    assert "[Debug] F10GameplayExit=1" in lines
    assert "[Keyboard] Jump=SPACE" in lines


def test_repair_keeps_f10_verbose_trace_and_native_exit(recover_runner,tmp_path):
    lines=recover(recover_runner,tmp_path,
        "[Debug]\nLog=1\nF10TestMode=1\nF10Auto3DScripts=1\n"
        "F10GameplayExit=1\nF10VerboseTrace=1\n[Unknown]\nValue=3\n")
    assert "[Debug] F10VerboseTrace=1" in lines
    assert "[Debug] F10GameplayExit=1" in lines

def test_recovery_preserves_hidden_native_esc_debug_optouts(recover_runner,tmp_path):
    # Manual overrides are not factory settings, but valid 0/1 HUD keys.
    ini=("[HUD]\nHUDWidescreenFix=1\nNativeESC4x3=0\n"
         "NativeMotif4x3=0\nNativeCursor4x3=0\n"
         "[Audio]\nVoiceLevel=72\n[Unknown]\nFoo=invalid\n")
    lines=recover(recover_runner,tmp_path,ini)
    for key in ("HUDWidescreenFix","NativeESC4x3","NativeMotif4x3","NativeCursor4x3"):
        value="1" if key=="HUDWidescreenFix" else "0"
        assert f"[HUD] {key}={value}" in lines
    assert "[Audio] VoiceLevel=72" in lines
    assert not any("[Unknown]" in line for line in lines)


