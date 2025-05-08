#
import sys
import signal
from time import sleep

sys.path.append("relay_lib")
from relay_lib.test_comm import testerApi
from relay_lib.relay_control import relayControl

sys.path.append("app_lib")
from relay_lib.app_utils import list_comm_devices, get_usb_sn

def sig_handler(signum, frame):
    sys.exit(0)

def fix_setup(tty_dev:str) -> testerApi|None:
    '''Initialize communication with the fixture'''
    fix: testerApi = testerApi(tty_dev)

    if not fix.open():
        print("Failed to open communication with fixture")
        return None
    fix.reset()

    for _ in range(8):
        sleep(0.2)
        ver = fix.fw_version()
        if ver:
            break
    if ver is None:
        fix.close()
        print("Failed to communicate with fixture")
        return None
    print(f"Fixture version {ver}")

    for _ in range(2):
        success = fix.baud_set(921600)
        if not success:
            sleep(0.1)
            continue
    if not success:
        print("Failed to change baud rate")
        return None

    # Echo test at new baud rate
    test_str = "Hello fixture"
    resp = fix.echo(test_str)
    if resp != test_str:
        fix.close()
        print("Failed contact fixture")
        return None

    # return fixture handle
    return fix

def main() -> int:
    print("Test relay board")

    signal.signal(signal.SIGINT, sig_handler)

    # Check for attached relay fixture
    # ESP32-S3 DevKit board
    dev_vid = 0x1a86
    dev_pid = 0x55d3
    fix_devs: list[str] = list_comm_devices(vid=dev_vid, pid=dev_pid)
    num_fix: int = len(fix_devs)
    if num_fix == 0:
        print("No test fixtures found")
        return 1
    if num_fix > 1:
        print(f"Too many ({num_fix}) test fixtures found, must be 1")
        return 1

    tty_dev: str = fix_devs[0]
    dev_sn = get_usb_sn(dev_vid, dev_pid)[0]

    print(f"Found fixture at '{tty_dev}', serial number: '{dev_sn}'")

    # Set up communication with the fixture
    relay_board: testerApi = fix_setup(tty_dev)
    if relay_board is None:
        return 1

    # Set up relay control
    relay_ctrl: relayControl = relayControl(relay_board)

    # Test each relay
    for relay_num in range(1, 9):
        sleep(0.4)
        print(f"Turn on relay {relay_num}")
        relay_ctrl.set_relay(relay_num, True)

        sleep(0.4)
        print(f"Turn off relay {relay_num}")
        relay_ctrl.set_relay(relay_num, False)

    return 0

if __name__ == "__main__":
    ret: int = main()
    sys.exit(ret)
