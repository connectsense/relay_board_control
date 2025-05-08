from test_comm import testerApi
from relay_lib.board_control import boardControl

class relayControl(boardControl):
    '''Control the relay board'''
    def __init__(self, fix_api:testerApi) -> None:

        # This table maps functional names to GPIOs on ESP32-S3
        gpio_map: list[dict[str, int, str, bool]] = [
            {"name": "relay-1", "gpio_num":  4, "dir": "out", "active_hi": False}, # Relay 1
            {"name": "relay-2", "gpio_num":  5, "dir": "out", "active_hi": False}, # Relay 2
            {"name": "relay-3", "gpio_num":  6, "dir": "out", "active_hi": False}, # Relay 3
            {"name": "relay-4", "gpio_num":  7, "dir": "out", "active_hi": False}, # Relay 4
            {"name": "relay-5", "gpio_num": 15, "dir": "out", "active_hi": False}, # Relay 5
            {"name": "relay-6", "gpio_num": 16, "dir": "out", "active_hi": False}, # Relay 6
            {"name": "relay-7", "gpio_num": 17, "dir": "out", "active_hi": False}, # Relay 7
            {"name": "relay-8", "gpio_num": 18, "dir": "out", "active_hi": False}  # Relay 8
        ]

        super().__init__(fix_api, gpio_map=gpio_map)
        self.initialize(dbug=False)

    def set_relay(self, relay_num:int, active:bool, dbug:bool=False) -> bool:
        if relay_num < 1 or relay_num > 8:
            return False
        name = f"relay-{relay_num}"
        return self.gpio_set(name, active, dbug=dbug)
