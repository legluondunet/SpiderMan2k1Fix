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
