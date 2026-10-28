"""Regression: unique raw settings, coherent defaults, no obsolete sections."""
from pathlib import Path
import configparser
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
ACTIONS = ("Up","Down","Left","Right","Jump","Kick","Punch","Web","Aim","Zipline","Swing")
SCANS = (0xC8,0xD0,0xCB,0xCD,0x39,0x2D,0x2E,0x2F,0x20,0x21,0x22)


def read_ini(filename):
    data=(ROOT / filename).read_text()
    p=configparser.ConfigParser(interpolation=None)
    p.read_string(data)
    assert data.count("\nRawHex=") == 1
    assert len(p['Internal']['RawHex']) == 504
    expected = ({'Debug','Display','Input','Audio','Keyboard','Joystick','Internal'}
                if 'generated-example' in filename else
                {'Debug','Display','HUD','Input','Audio','Keyboard','Joystick','Internal'})
    assert set(p.sections()) == expected
    assert not p.has_section('System')
    for obsolete in ('Controls','GameConfig','Gamepad'):
        assert not p.has_section(obsolete)
    raw=bytes.fromhex(p['Internal']['RawHex'])
    assert len(raw)==252
    for i,(key,code) in enumerate(zip(ACTIONS,SCANS)):
        assert struct.unpack_from('<I',raw,0x14+i*4)[0] == code, key
    # User-clean is an existing personalized INI; changing factory defaults
    # must NOT silently rewrite that user's saved audio preferences.
    old_preferences = 'user-clean' in filename
    assert struct.unpack_from('<I',raw,0xEC)[0] == (0x2BFF if old_preferences else 0x1999)
    assert struct.unpack_from('<I',raw,0xF0)[0] == (177 if old_preferences else 102)
    assert struct.unpack_from('<I',raw,0xF4)[0] == (201 if old_preferences else 153)
    assert struct.unpack_from('<I',raw,0xF8)[0] == 0
    assert [p['Audio'][k] for k in ('MusicSFXLevel','VoiceLevel','MovieLevel','Stereo')] == (['69','79','69','1'] if old_preferences else ['40','60','40','1'])
    assert [p['Keyboard'][k] for k in ACTIONS] == ['UP','DOWN','LEFT','RIGHT','SPACE','X','C','V','D','F','G']
    for i,key in enumerate(('Jump','Kick','Punch','Web','Aim','Zipline','Swing')):
        assert p['Joystick'][key] == f'BUTTON{struct.unpack_from("<I",raw,0x50+i*4)[0]}'
    return raw


def test_shipped_examples_and_rawhex():
    factory=bytes(int(v,16) for v in re.findall(
        r'0x([0-9a-fA-F]{2})',
        (ROOT/'spidey_cfg_template.h').read_text().split('{',1)[1].split('}',1)[0]))
    assert factory==read_ini('ChatGPT/tests/fixtures/SpiderMan2k1Fix.ini')
    assert factory==read_ini('ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini')
    assert read_ini('ChatGPT/tests/fixtures/SpiderMan2k1Fix.user-clean.ini')[0x50] == 2
    assert factory[0x50] == 3


def test_runtime_generator_uses_factory_and_one_internal_section():
    src=(ROOT/'virtual_spidey_cfg.c').read_text()
    assert 'memcpy(cfg,spidey_cfg_factory,CFG_SIZE);' in src
    assert 'spidey_cfg_factory[off]' in src
    assert 'for(i=0;i<CFG_SIZE;i++)sprintf(hex+i*2,' in src
    assert r'"\r\n[Internal]\r\n"' in src
    assert src.count('"RawHex=";') == 1
    assert r'"\r\n[Keyboard]\r\n"' in src
    assert r'"\r\n[Joystick]\r\n"' in src
    assert r'"\r\n[Controls]\r\n"' not in src
    assert 'CreateFileA(ini,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,' in src
