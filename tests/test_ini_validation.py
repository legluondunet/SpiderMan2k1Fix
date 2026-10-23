"""Read-only validation of valid, unknown, damaged and duplicated settings."""
from pathlib import Path
import re
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope="module")
def runner(tmp_path_factory):
    if not shutil.which("gcc"):
        pytest.skip("host C compiler missing")
    dest = tmp_path_factory.mktemp("ini-validator") / "test_ini_validator"
    subprocess.run(["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
                    str(ROOT / "tests/test_ini_validator_runner.c"), "-o", str(dest)],
                   check=True, capture_output=True, text=True)
    return dest


def validate(runner, tmp_path, contents):
    p = tmp_path / "SpiderMan2k1Fix.ini"
    p.write_text(contents, encoding="ascii")
    r = subprocess.run([str(runner), str(p)], capture_output=True, text=True, check=True)
    results = tuple(map(int,re.search(r"RESULT:(\d+):(\d+):(\d+)",r.stdout).groups()))
    return results, r.stdout


def test_valid_generated_ini(runner, tmp_path):
    data = (ROOT / "SpiderMan2k1Fix.generated-example.ini").read_text()
    assert validate(runner,tmp_path,data)[0] == (0,0,1)


def test_native_directinput_retry_option_and_legacy_ini_compat(runner, tmp_path):
    # New key is boolean and opt-in. Old quarantined COM flags must not cause
    # the user's audio and keyboard options to be reset by INI repair.
    result, _ = validate(runner, tmp_path,
        "[Input]\nDirectInputReacquire=1\nPauseOnFocusLoss=1\n"
        "PauseKey=Any\nExperimentalAltTabReacquire=0\n"
        "ExperimentalDI8CreateDeviceTrace=0\n")
    assert result == (0, 0, 1)
    for value in ("2", "-1", "true", "off"):
        result, _ = validate(runner, tmp_path,
            f"[Input]\nDirectInputReacquire={value}\n")
        assert result[0] >= 1 and result[2] == 0



@pytest.mark.parametrize("key", ["PauseBinkOnFocusLoss", "PauseDirectSoundOnFocusLoss"])
def test_media_pause_options_accept_only_boolean_values(runner, tmp_path, key):
    # Both media modules are opt-in and share the existing Input INI validator.
    for value in ("0", "1"):
        result, _ = validate(runner, tmp_path, f"[Input]\n{key}={value}\n")
        assert result == (0, 0, 1)
    for value in ("-1", "2", "true", "enabled"):
        result, _ = validate(runner, tmp_path, f"[Input]\n{key}={value}\n")
        assert result[0] >= 1 and result[2] == 0



def test_pause_overlay_ini_option_is_boolean(runner, tmp_path):
    for value in ("0", "1"):
        result, _ = validate(runner, tmp_path, f"[Input]\nPauseOverlay={value}\n")
        assert result == (0, 0, 1)
    for value in ("2", "-1", "true", "off"):
        result, _ = validate(runner, tmp_path, f"[Input]\nPauseOverlay={value}\n")
        assert result[0] >= 1 and result[2] == 0


@pytest.mark.parametrize("value,valid",[("0",True),("1",True),("2",False),("-1",False),("on",False)])
def test_windows_ghosting_option_strict_boolean(runner, tmp_path, value, valid):
    # Experimental process-wide compatibility option. Never enable implicitly
    # or accept ambiguous values that might alter Windows hang behaviour.
    result, _ = validate(runner,tmp_path,f"[Input]\nDisableWindowsGhosting={value}\n")
    assert (result==(0,0,1)) if valid else (result[0]>=1 and result[2]==0)


@pytest.mark.parametrize("value,valid",[("0",True),("1",True),("2",False),("-1",False),("true",False)])
def test_pause_dim_ini_option_is_boolean(runner, tmp_path, value, valid):
    result, _ = validate(runner,tmp_path,f"[Input]\nPauseDim={value}\n")
    if valid:
        assert result == (0,0,1)
    else:
        assert result[0] >= 1 and result[2] == 0


@pytest.mark.parametrize("value,valid",[
    ("0",True),("1",True),("55",True),("90",True),
    ("91",False),("100",False),("-1",False),("55%",False),("auto",False)
])
def test_pause_dim_opacity_strict_0_to_90(runner, tmp_path, value, valid):
    result, _ = validate(runner,tmp_path,f"[Input]\nPauseDimOpacity={value}\n")
    if valid:
        assert result==(0,0,1)
    else:
        assert result[0]>=1 and result[2]==0


@pytest.mark.parametrize("value,valid",[
    ("0",True),("1",True),("2",False),("-1",False),("yes",False)
])
def test_pause_grid_only_boolean(runner, tmp_path, value, valid):
    result, _ = validate(runner,tmp_path,f"[Input]\nPauseGrid={value}\n")
    if valid:
        assert result==(0,0,1)
    else:
        assert result[0]>=1 and result[2]==0


def test_unknown_section_and_key_preserved(runner, tmp_path):
    result,detail=validate(runner,tmp_path,"[Debug]\nLog=0\nUnknownKey=20\n[Plugin]\nFoo=Bar\n")
    assert result == (0,2,1)
    assert "Unknown section [Plugin]" in detail
    assert "Unknown key [Debug] UnknownKey" in detail


def test_duplicate_key_blocks_save(runner,tmp_path):
    result,detail=validate(runner,tmp_path,"[Audio]\nVoiceLevel=79\nVoiceLevel=80\n")
    assert result == (1,0,0)
    assert "Duplicate key" in detail


def test_duplicate_section_blocks_save(runner,tmp_path):
    result,_=validate(runner,tmp_path,"[Keyboard]\nJump=SPACE\n[Keyboard]\nPunch=C\n")
    assert result == (1,0,0)


def test_bad_hex_and_legacy_rawhex_block_save(runner,tmp_path):
    result,detail=validate(runner,tmp_path,"[Internal]\nRawHex=ABC\n[GameConfig]\nRawHex=AAA\n")
    assert result[0] == 2 and result[1] == 1 and result[2] == 0
    assert "RawHex must appear only" in detail

@pytest.mark.parametrize("data",[
    "[Audio]\nMusicSFXLevel=101\n", "[Audio]\nStereo=2\n",
    "[Keyboard]\nJump=SPCAE\n", "[Joystick]\nJump=BUTTON999\n",
    "[Display]\nResolution=garbage\n", "[Display]\nBrightness=-1\n",
    "[Display]\nFPSLimit=900\n", "[Debug]\nLog=banana\n",
    "[Keyboard\nJump=F\n", "garbage without section\n"
])
def test_invalid_data_detected(runner,tmp_path,data):
    result,_=validate(runner,tmp_path,data)
    assert result[0]>=1 and result[2]==0


def test_bom_allowed(runner,tmp_path):
    p=tmp_path/"SpiderMan2k1Fix.ini"
    p.write_bytes(b"\xef\xbb\xbf[Debug]\nLog=1\n")
    output=subprocess.run([str(runner),str(p)],capture_output=True,text=True,check=True).stdout
    assert "RESULT:0:0:1" in output


def test_binary_null_is_error(runner,tmp_path):
    p=tmp_path/"SpiderMan2k1Fix.ini"
    p.write_bytes(b"[Audio]\nVoiceLevel=79\x00extra\n")
    output=subprocess.run([str(runner),str(p)],capture_output=True,text=True,check=True).stdout
    assert "Binary NUL byte" in output
    assert "RESULT:1:0:0" in output


def test_integration_blocks_saving_before_writing():
    src=(ROOT/"virtual_spidey_cfg.c").read_text()
    loader=(ROOT/"spiderman2k1fix.c").read_text()
    assert loader.index("SpiderCfg_EnsureDefaultIni(log_line)") < loader.index("SpiderCfg_ValidateCurrentIni(log_line)")
    assert loader.index("SpiderCfg_ValidateCurrentIni(log_line)") < loader.index('GetPrivateProfileStringA("Display","FPSLimit"')
    assert src.index("if(!ini_writes_permitted)") < src.index('WritePrivateProfileStringA("Internal","RawHex"')
    assert "MessageBoxA(NULL,summary" in src
    assert "SpiderMan2k1Fix_" in src
    assert "SpiderCfg_RepairIni" in src


def test_system_quick_exit_is_known_and_opt_in(runner, tmp_path):
    for value in ("0", "1"):
        result, _ = validate(runner, tmp_path, f"[System]\nQuickExitF12={value}\n")
        assert result == (0, 0, 1)
    for value in ("2", "-1", "true", "off"):
        result, _ = validate(runner, tmp_path, f"[System]\nQuickExitF12={value}\n")
        assert result[0] >= 1 and result[2] == 0


def test_example_exposes_disabled_quick_exit():
    data = (ROOT / "SpiderMan2k1Fix.generated-example.ini").read_text()
    assert "[System]\n" in data
    assert "QuickExitF12=0" in data


@pytest.mark.parametrize("value,valid", [
    ("0", True), ("1", True),
    ("2", False), ("-1", False), ("true", False), ("enabled", False), ("", False)
])
def test_f12_auto_3d_scripts_key_is_validated_as_strict_debug_boolean(
    runner, tmp_path, value, valid
):
    """Regression: a known opt-in must never be treated as an unknown
    key and silently dropped by automatic INI recovery."""
    config = f"[Debug]\\nLog=1\\nF12TestMode=1\\nF12Auto3DScripts={value}\\n"
    result, detail = validate(runner, tmp_path, config)
    if valid:
        assert result == (0, 0, 1), detail
    else:
        assert result[0] >= 1 and result[2] == 0, detail


def test_complete_3d_opt_in_is_accepted_and_example_defaults_off(runner, tmp_path):
    ini = (ROOT / "SpiderMan2k1Fix.generated-example.ini").read_text()
    assert "F12Auto3DScripts=0" in ini
    assert validate(runner, tmp_path, ini)[0] == (0, 0, 1)
    enabled = ini.replace("Log=0", "Log=1", 1).replace(
        "F12TestMode=0", "F12TestMode=1", 1
    ).replace("F12Auto3DScripts=0", "F12Auto3DScripts=1", 1)
    assert validate(runner, tmp_path, enabled)[0] == (0, 0, 1)
