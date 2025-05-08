#
import sys
from time import sleep
from argparse import ArgumentParser

sys.path.append("relay_lib")
from relay_lib.relay_control import relayControl
from relay_lib.test_comm import testerApi

def main() -> int:
    print("Demonstrate relay board control")

    argp = ArgumentParser()
    argp.add_argument("port", type=str, metavar="PORT", help="Serial port name")
    args = argp.parse_args()

    tester: testerApi = testerApi(args.port)
    tester.open()
    fix: relayControl = relayControl(tester)

    print("Cycle through the relays")
    for relay_num in range(1, 9):
        sleep(0.5)
        print(f"Relay {relay_num} On")
        fix.set_relay(relay_num, True)
        sleep(0.5)
        print(f"Relay {relay_num} Off")
        fix.set_relay(relay_num, False)

    tester.close()
    print("Done!")
    return 0

if __name__ == "__main__":
    ret: int = main()
    sys.exit(ret)
