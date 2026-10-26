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

def test_repair_uses_exactly_one_rolling_old_file_and_rolls_back():
    src=(ROOT/'virtual_spidey_cfg.c').read_text()
    assert 'SpiderCfg_WriteDefaultIniAt(tmp,0,logger)' in src
    assert 'SpiderIni_Recover(ini,values)' in src
    assert 'snprintf(backup,sizeof(backup),"%s.old",ini)' in src
    assert 'MoveFileExA(ini,backup,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)' in src
    assert 'MoveFileExA(tmp,ini,MOVEFILE_WRITE_THROUGH)' in src
    assert 'MoveFileExA(backup,ini,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)' in src
    assert 'CopyFileA(backup,ini,TRUE)' in src
    assert 'SpiderCfg_PruneNumberedOldFiles(directory,logger);' in src
    assert 'FindFirstFileA(pattern,&found)' in src
    assert 'strcmp(p,".old")!=0' in src
    # First fully construct/validate the replacement, then rename the old INI.
    assert src.index('SpiderIni_Validate(tmp,NULL,NULL)') < src.index(
        'MoveFileExA(ini,backup,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)')
    assert src.index('MoveFileExA(ini,backup,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)') < src.index(
        'MoveFileExA(tmp,ini,MOVEFILE_WRITE_THROUGH)')
    assert 'SpiderMan2k1Fix_%u.old' not in src
    assert 'CopyFileA(ini,backup,TRUE)' not in src
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




def test_recovery_discards_retired_grid_but_keeps_user_pause_opacity(recover_runner,tmp_path):
    ini=("[Input]\nPauseGrid=1\nPauseDimOpacity=78\n"
         "[Display]\nFPSLimit=45\nAspectRatio=4:3\n"
         "[Keyboard]\nJump=SPACE\n[Audio]\nVoiceLevel=72\n")
    values=recover(recover_runner,tmp_path,ini)
    assert "[Input] PauseDimOpacity=78" in values
    assert "[Display] FPSLimit=45" in values
    assert "[Display] AspectRatio=4:3" in values
    assert "[Keyboard] Jump=SPACE" in values
    assert "[Audio] VoiceLevel=72" in values
    assert not any("PauseGrid" in line for line in values)


def test_optional_zero_shade_survives_recovery(recover_runner,tmp_path):
    values=recover(recover_runner,tmp_path,
                   "[Input]\nPauseDimOpacity=0\nPauseGrid=0\n")
    assert "[Input] PauseDimOpacity=0" in values
    assert not any("PauseGrid" in line for line in values)


@pytest.mark.parametrize("duplicate_section", [False, True])
def test_audio_voice_trace_survives_recovery_of_legacy_ini(recover_runner, tmp_path, duplicate_section):
    """Preserve explicitly enabled diagnostic on unrelated startup migration/repair."""
    ini = ("[Debug]\nLog=1\n"
           + (("[Debug]\n" if duplicate_section else ""))
           + "ExperimentalAudioVoiceTrace=1\n"
           "[Audio]\nMusicSFXLevel=60\nVoiceLevel=0\n"
           "[Input]\nKeyboardLayout=Auto\n"
           "[Display]\nResolution=1920x1080\n"
           "[Extra]\nUnsupported=1\n")
    values = recover(recover_runner, tmp_path, ini)
    assert "[Debug] Log=1" in values
    assert "[Debug] ExperimentalAudioVoiceTrace=1" in values
    assert "[Audio] MusicSFXLevel=60" in values
    assert "[Audio] VoiceLevel=0" in values
    assert "[Input] KeyboardLayout=Auto" in values
    assert not any("Extra" in value for value in values)


def test_audio_voice_trace_recover_first_valid_value(recover_runner, tmp_path):
    values = recover(recover_runner, tmp_path,
                     "[Debug]\nExperimentalAudioVoiceTrace=invalid\n"
                     "ExperimentalAudioVoiceTrace=1\n")
    assert values == ["[Debug] ExperimentalAudioVoiceTrace=1"]


def test_audio_loop_stop_repair_preserves_audio_trace_and_modding(recover_runner,tmp_path):
    ini=("[Debug]\nLog=1\nExperimentalAudioVoiceTrace=1\n"
         "ExperimentalAudioStopSuspectLoop=1\n"
         "[Modding]\nLooseFiles=1\n"
         "[Audio]\nMusicSFXLevel=60\nVoiceLevel=0\n"
         "[InvalidSection]\nWrong=1\n")
    values=recover(recover_runner,tmp_path,ini)
    for option in ("[Debug] ExperimentalAudioVoiceTrace=1",
                   "[Debug] ExperimentalAudioStopSuspectLoop=1",
                   "[Modding] LooseFiles=1",
                   "[Audio] MusicSFXLevel=60",
                   "[Audio] VoiceLevel=0"):
        assert option in values
    assert not any("InvalidSection" in line for line in values)


def test_loop_mask_repair_preserves_old_diag_and_modding(recover_runner,tmp_path):
    ini=("[Debug]\nLog=1\nExperimentalAudioStopSuspectLoop=1\n"
         "ExperimentalAudioVoiceTrace=1\nExperimentalAudioStopLoopMask=30\n"
         "[Modding]\nLooseFiles=1\n[Audio]\nMusicSFXLevel=60\nVoiceLevel=0\n"
         "[Extraneous]\nTrash=1\n")
    values=recover(recover_runner,tmp_path,ini)
    for key in ("[Debug] ExperimentalAudioStopLoopMask=30",
                "[Debug] ExperimentalAudioStopSuspectLoop=1",
                "[Debug] ExperimentalAudioVoiceTrace=1",
                "[Modding] LooseFiles=1",
                "[Audio] MusicSFXLevel=60","[Audio] VoiceLevel=0"):
        assert key in values


def test_oneshot_trace_recovery_preserves_debug_and_modding(recover_runner,tmp_path):
    ini=("[Debug]\nLog=1\nExperimentalAudioOneShotTrace=1\n"
         "ExperimentalAudioVoiceTrace=1\nExperimentalAudioStopLoopMask=30\n"
         "ExperimentalAudioStopSuspectLoop=1\n"
         "[Modding]\nLooseFiles=1\n"
         "[BadStuff]\nInvalid=1\n")
    values=recover(recover_runner,tmp_path,ini)
    for key in ("[Debug] ExperimentalAudioOneShotTrace=1",
                "[Debug] ExperimentalAudioVoiceTrace=1",
                "[Debug] ExperimentalAudioStopLoopMask=30",
                "[Debug] ExperimentalAudioStopSuspectLoop=1",
                "[Modding] LooseFiles=1"):
        assert key in values


def test_native_audio_trace_survives_ini_repair(recover_runner,tmp_path):
    original=("[Debug]\nLog=1\nExperimentalAudioNativeLifecycleTrace=1\n"
              "ExperimentalAudioOneShotTrace=1\n"
              "ExperimentalAudioStopLoopMask=30\n"
              "[Keyboard]\nJump=SPACE\n"
              "[Unknown]\nFake=1\n")
    lines=recover(recover_runner,tmp_path,original)
    for item in ("[Debug] ExperimentalAudioNativeLifecycleTrace=1",
                 "[Debug] ExperimentalAudioOneShotTrace=1",
                 "[Debug] ExperimentalAudioStopLoopMask=30",
                 "[Keyboard] Jump=SPACE"):
        assert item in lines
