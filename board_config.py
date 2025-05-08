#
import sys
from time import sleep
from argparse import ArgumentParser

sys.path.append("relay_lib")
from relay_lib.test_comm import testerApi
from relay_lib.relay_control import relayControl

sys.path.append("app_lib")
from relay_lib.app_utils import list_comm_devices, get_usb_sn

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
    for _ in range(4):
        resp = fix.echo(test_str)
        if resp == test_str:
            break
        sleep(0.2)
    if resp != test_str:
        fix.close()
        print("Failed contact fixture")
        return None

    # return fixture handle
    return fix

def main() -> int:
    print("Configure the relay board")

    # Check for attached board (ESP32-S3 DevKit)
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

    comm_dev: str = fix_devs[0]
    dev_sn = get_usb_sn(dev_vid, dev_pid)[0]

    print(f"Found fixture at '{comm_dev}', serial number: '{dev_sn}'")

    argp = ArgumentParser()
    argp.add_argument("--unit_sn", type=str, required=True, help="Serial number to assign board")
    args = argp.parse_args()

    # Set up communication with the fixture
    relay_board: testerApi = fix_setup(comm_dev)
    if relay_board is None:
        return 1

    # Set up relay control
    relay_ctrl: relayControl = relayControl(relay_board)

    # Read current parameters from board
    board_params = relay_ctrl.config_get()
    sn = board_params.get("unit_sn")
    if sn == args.unit_sn:
        print("Board serial number already set")
        return 0

    print(f"Set board serial number to {args.unit_sn}")
    relay_ctrl.config_set({'unit_sn': args.unit_sn})
    board_params = relay_ctrl.config_get()
    sn = board_params.get("unit_sn")
    if sn == args.unit_sn:
        return 0

    print("Failed to set serial number")
    return 1

if __name__ == "__main__":
    ret: int = main()
    sys.exit(ret)
