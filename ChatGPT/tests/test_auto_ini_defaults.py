"""Static and data-regression checks; cannot replace a Wine/game run."""
from pathlib import Path
import configparser
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
virtual = (ROOT / 'virtual_spidey_cfg.c').read_text()
game = (ROOT / 'spiderman2k1fix.c').read_text()
example = (ROOT / 'ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini').read_text()
factory = (ROOT / 'spidey_cfg_template.h').read_text()
bytes_data = bytes(int(v,16) for v in re.findall(r'0x([0-9a-fA-F]{2})',factory.split('{',1)[1].split('}',1)[0]))
config = configparser.ConfigParser(interpolation=None)
config.read_string(example)


def rounded_pct(raw, maximum):
    return min(100,(raw*100 + maximum//2)//maximum)


def to_raw(pct, maximum):
    return (pct*maximum + 50)//100


def test_generated_example_factory_consistency():
    assert len(bytes_data) == 252
    assert bytes.fromhex(config['Internal']['RawHex']) == bytes_data
    assert set(config.sections()) == {'Debug','Display','HUD','Input','Audio','Keyboard','Joystick','Internal'}
    assert config['Debug']['Log'] == '0'
    for key in ('F10Shortcut','F10TestMode','F10Auto3DScripts','F10GameplayExit'):
        assert not config.has_option('Debug',key)
        assert f'"{key}=0\\r\\n"' not in virtual
        assert f'{key}=' not in (ROOT / 'ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini').read_text()
    assert config['Debug']['F10VerboseTrace'] == '0'
    assert '"F10VerboseTrace=0\\r\\n"' in virtual
    assert not config.has_section('System')
    assert not config.has_option('Internal','VirtualSpideyCfg')
    assert not config.has_option('Internal','BootstrapGameConfig')
    assert config['Display']['Resolution'] == 'Auto'
    assert config['Display']['Brightness'] == '4'
    assert config['Display']['FPSLimit'] == '30'
    assert config['Input']['DirectInputReacquire'] == '0'
    assert config['Input']['PauseOnFocusLoss'] == '0'
    assert 'PauseKey' not in config['Input']  # Any-key resume is fixed, not selectable
    assert not config.has_option('Input','ExperimentalAltTabReacquire')
    assert not config.has_option('Input','ExperimentalDI8CreateDeviceTrace')
    assert config['Audio']['Stereo'] == '1'
    for key,off,maxval in [('MusicSFXLevel',236,16383),('VoiceLevel',244,255),('MovieLevel',240,255)]:
        raw = struct.unpack_from('<I',bytes_data,off)[0]
        assert config.getint('Audio',key) == rounded_pct(raw,maxval)
    assert config['Keyboard']['Jump']=='SPACE'
    assert config['Keyboard']['Up']=='UP'
    assert config['Keyboard']['Swing']=='G'
    assert [config['Keyboard'][key] for key in ('Kick','Punch','Web','Aim','Zipline','Swing')] == ['X','C','V','D','F','G']
    assert [config.getint('Audio',k) for k in ('MusicSFXLevel','VoiceLevel','MovieLevel')] == [69,79,69]
    assert not config.has_section('Gamepad')
    assert config['Joystick']['Jump'] == 'BUTTON3'
    assert config['Joystick']['Punch'] == 'BUTTON0'
    assert not config.has_section('GameConfig')
    assert not config.has_section('Controls')
    assert example.count('\nRawHex=') == 1
    assert 'GameConfig' not in example and '[Controls]' not in example
    assert 'GetPrivateProfileStringA("Keyboard",action_names[i]' in virtual
    assert 'WritePrivateProfileStringA("Keyboard",action_names[i]' in virtual
    assert 'GetPrivateProfileStringA("Joystick",joystick_actions[i]' in virtual
    assert 'GetPrivateProfileStringA("Internal","RawHex"' in virtual
    assert 'WritePrivateProfileStringA("Internal","RawHex"' in virtual


def test_round_trip_keeps_existing_raw_on_equal_percent():
    for key,off,maxval in [('MusicSFXLevel',236,16383),('VoiceLevel',244,255),('MovieLevel',240,255)]:
        for raw in (0,1,3,maxval//3,maxval//2,maxval-1,maxval):
            pct = rounded_pct(raw,maxval)
            unchanged = raw if rounded_pct(raw,maxval)==pct else to_raw(pct,maxval)
            assert unchanged == raw, (key,raw)
        assert to_raw(0,maxval) == 0
        assert to_raw(100,maxval) == maxval
    assert 'to_percent(raw,audio_volumes[i].max_value)==(unsigned)pct) continue;' in virtual
    assert 'if(!save_audio()) goto fail;' in virtual
    assert 'load_audio();' in virtual
    assert 'pct==1?0u:1u' in virtual


def test_create_before_read_and_never_overwrite():
    assert 'CreateFileA(ini,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,' in virtual
    assert 'error==ERROR_FILE_EXISTS || error==ERROR_ALREADY_EXISTS' in virtual
    assert 'DeleteFileA(ini)' in virtual
    assert game.index('if (!supported_exe())') < game.index('SpiderCfg_EnsureDefaultIni(log_line);')
    assert game.index('SpiderCfg_EnsureDefaultIni(log_line);') < game.index('GetPrivateProfileStringA("Display","FPSLimit"')
