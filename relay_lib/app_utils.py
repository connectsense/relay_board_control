from serial.tools import list_ports, list_ports_common
import re

def get_usb_sn(vid:str|int|None=None, pid:str|int|None=None) -> list[str]:
    '''Return a list of serial number(s) of the attached USB com port(s) matching VID and PID'''
    if type(vid) is str:
        vid: int = int(vid, 16)
    if type(pid) is str:
        pid: int = int(pid, 16)
    pl: list = list_ports.comports()
    p: list_ports_common.ListPortInfo = None
    ret: list[str] = list()
    for p in pl:
        if vid != p.vid:
            continue
        if pid != p.pid:
            continue
        ret.append(p.serial_number)
    return ret

def list_comm_devices(vid:int|str|None=None, pid:int|str|None=None, sn:str|None=None) -> list[str]:
    '''
    List USB com port devices matching the VID, PID, and serial number criteria
    
    Parameters
      vid : USB Vender ID, use None for don't care
      pid : USB Product ID, use None for don't care
      sn  : Device serial number, use None for don't care

    Return
      List of names of devices matching the given criteria
    '''
    if type(vid) is str:
        vid: int = int(vid, 16)
    if type(pid) is str:
        pid: int = int(pid, 16)

    ret: list[str] = []
    pl: list = list_ports.comports()
    p: list_ports_common.ListPortInfo = None
    for p in pl:
        if vid is not None and vid != p.vid:
            continue
        if pid is not None and pid != p.pid:
            continue
        if sn is not None and not p.serial_number.startswith(sn):
            continue
        ret.append(p.device)
    return sorted(ret)

def compare_versions(ver1:str, ver2:str) -> int|None:
    '''
    Compare two version strings of the form "n.n.n"

    Arguments
      ver1, ver2 : The two strings to compare

    return
      -1    : ver1 is less than ver2
       0    : ver2 is equal to ver2
       1    : ver2 is greater than ver2
       None : One or both strings are not the proper form
    '''
    # Check the version strings for proper form
    vcheck: re.Pattern = re.compile(r"^[0-9]*\.[0-9]*\.[0-9]$")

    if vcheck.match(ver1) is None:
        return None
    if vcheck.match(ver2) is None:
        return None

    # Now do the comparison
    vl1: list[int] = [int(v) for v in ver1.split('.')]
    vl1Len: int = len(vl1)
    vl2: list[int] = [int(v) for v in ver2.split('.')]
    vl2Len: int = len(vl2)
    vlen: int = max(vl1Len, vl2Len)
    for i in range(vlen):
        c1 = vl1[i] if i < vl1Len else 0
        c2 = vl2[i] if i < vl2Len else 0

        if c1 < c2:
            # version 1 is less than version 2
            return -1
        if c1 > c2:
            # version 1 is greater than version 2
            return 1
    # versions are equal
    return 0
