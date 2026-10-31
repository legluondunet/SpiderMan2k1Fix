"""DS4 game-local routing and INI policy: source-only regressions.

These checks do not establish actual Windows speaker output. A genuine DS4
and a native Windows run are still necessary before merge.
"""
from pathlib import Path
import subprocess
import shutil
import pytest

ROOT=Path(__file__).resolve().parents[2]

def test_policy_is_in_factory_ini_and_validated():
    fixture=(ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
    source=(ROOT/"virtual_spidey_cfg.c").read_text()
    validator=(ROOT/"ini_validation.h").read_text()
    assert "DualShock4AudioOutput=0" in fixture
    assert '"DualShock4AudioOutput=0\\r\\n"' in source
    assert '"DualShock4AudioOutput"' in validator
    assert '!spider_ini_icmp(key,"DualShock4AudioOutput")' in validator

@pytest.mark.parametrize("value,valid", [
    ("0",True),("1",True),("2",False),("-1",False),
    ("on",False),("off",False),("",False),("100",False),
])
def test_policy_is_strict_boolean(tmp_path,value,valid):
    if not shutil.which("gcc"):
        pytest.skip("C host compiler unavailable")
    exe=tmp_path/"validate_ini"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror",
          str(ROOT/"ChatGPT/tests/test_ini_validator_runner.c"),"-o",str(exe)],
          capture_output=True,text=True,check=True)
    p=tmp_path/"SpiderMan2k1Fix.ini"
    p.write_text("[Audio]\nDualShock4AudioOutput="+value+"\n")
    res=subprocess.run([str(exe),str(p)],capture_output=True,text=True,check=True)
    assert ("RESULT:0:0:1" in res.stdout)==valid, res.stdout

def test_no_global_audio_device_changes():
    src=(ROOT/"audio_ds4_route.c").read_text()
    game=(ROOT/"spiderman2k1fix.c").read_text()
    build=(ROOT/"build.sh").read_text()
    assert "DSDEVID_DefaultPlayback" in src
    assert "DirectSoundEnumerateA" in src
    assert "DirectSoundCreate8" in src
    assert "real_create8(&choices.fallback_guid,out,outer)" in src
    assert "real_create8(requested,out,outer)" in src
    assert "wine_get_version" in src
    assert "SpiderAudioDS4Route_Install(ini,log_line)" in game
    assert "audio_ds4_route.c" in build
    for dangerous in ("SetDefaultEndpoint","IPolicyConfig","SetupDiCallClassInstaller",
                      "DisableDevice","EnableDevice","SetMasterVolumeLevel"):
        assert dangerous not in src
