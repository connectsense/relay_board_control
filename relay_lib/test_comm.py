"""
The TesterComm class handles message communication between a test script and a
connected test module that is running test firmware.
"""
import platform
import serial
from time import time, sleep
import json
from threading import Lock
from crccheck.crc import Crc32

class testerComm:
    '''
    This communicates serially with the tester module. Messages are framed using ASCII
    SOH, STX, ETX, and EOT characters:

    SOH <header> STX <body> ETX <crc> EOT

    header is one of:
    CMD   body is a command from the test script to the module
    RESP  from module to test script. Body contains the result of the previous CMD
          message.
    ERR   body is an error response from the module to the script
          it reports a problem with the message framing: buffer overflows,
          invalid character, CRC validation failure, etc

    body  For CMD this is a 'thin' variant of JSON RPC, consisting of only "method" and optional
          "params"

    body  For RESP this is a 'thin' variant of JSON RPC, consisting of only "result" or "error"
          success with no data returned:
              {"result": 0}
          success with data returned:
              {"result": {"fun_level": 18}}   
          failure:
              {"error": -27, "message":"Not enough memory"}

    crc   is the hex representation of the 32-bit checksum of the message body
    '''
    def __init__(self, comm_dev:str, baud:int=115200, timeout:float=0.5) -> None:
        self._version: str = "1.0.0"
        self._failReason: str = ""

        self.comm_dev: str = comm_dev

        self.system: str = platform.system()
        self.mutex: Lock = Lock()

        self.port: serial.Serial = serial.Serial()
        self.port.port = comm_dev
        self.port.baudrate = baud
        self.port.timeout = timeout
        self.port.dtr = False
        self.port.rts = False

    def _fail(self, mesg:str, dbug:bool=False) -> None:
        '''Set fail reason string and, if debug is enabled, print it'''
        self._failReason = mesg
        if dbug:
            print(mesg)

    def _debug(self, mesg:str, dbug:bool=False) -> None:
        '''If debug is enabled, print message to the console'''
        if dbug:
            print(mesg)

    def spy(self, duration:float=30) -> None:
        '''Receive and print all serial data output by the uut'''
        endTime: float = time() + duration
        while time() < endTime:
            r = self.port.read(1)
            if len(r) == 0:
                continue
            try:
                rd = r.decode('UTF-8')
                print(rd, end="")
            except:
                print(r, end="")

    def _send_mesg(self, hdr:str, body:str, dbug:bool=False) -> bool:
        '''Send a message to the unit under test'''
        if self.port is None:
            self._fail(f"'{self.comm_dev}' is not open", dbug=dbug)
            return False

        hdr = hdr.encode("UTF-8")
        body = body.encode("UTF-8")

        # Compute CRC32 of message body
        crcInst = Crc32()
        crcInst.process(body)
        # convert CRC to hex string, trim off the first two characters (0x)
        crc = f"{crcInst.final():08x}".encode("UTF-8")

        # Message frame: SOH <header> STX <body> ETX <crc> EOT
        msg = b"\x01" + hdr + b"\x02" + body + b"\x03" + crc + b'\x04'

        if dbug:
            print(f"Send: {msg}")
        try:
            self.port.write(msg)
            self.port.flush()
        except Exception as e:
            self._fail(f"Failed to write to serial port: {e}", dbug=dbug)
            return False
        return True

    def _recv_mesg(self, timeout:float=5, dbug:bool=False) -> tuple|None:
        '''Receive a message from the unit under test'''
        hdr = b""
        body = b""
        crc = b""
        state = 0

        endTime = time() + timeout
        while time() < endTime:
            r = self.port.read(1)
            if len(r) == 0:
                continue

            if dbug:
                try:
                    print(r.decode('utf-8'), end="")
                except:
                    print(r, end="")

            if 0 == state:
                # Waiting for SOH
                if b'\x01' == r:
                    state = 1
            elif 1 == state:
                # Reading the header
                if b'\x01' == r:
                    # Restart header
                    hdr = b""
                elif b'\x02' == r:
                    # Begin message body
                    state = 2
                elif b'\x03' == r:
                    # End of body -- header with no body, wait for EOT
                    state = 3
                elif b'\x04' == r:
                    # End of message -- early termination
                    state = 0
                elif r >= b'\x20' and r <= b'\xfe':
                    # Store header character
                    hdr += r
                else:
                    # Invalid character
                    self._debug(f"Invalid header character ({r}) received", dbug=dbug)
                    state = 0
            elif 2 == state:
                # Receiving message body
                if b'\x01' == r:
                    # Restart header
                    hdr = b""
                    state = 1
                elif b'\x02' == r:
                    # Restart body
                    body = b""
                elif b'\x03' == r:
                    # End of body, receive CRC
                    state = 3
                elif b'\x04' == r:
                    # Early termination of message
                    state = 0
                elif (r >= b'\x20' and r <= b'\xfe'):
                    # Store body character
                    body += r
                else:
                    # Invalid character
                    self._debug(f"Invalid body character ({r}) received", dbug=dbug)
                    state = 0
            elif 3 == state:
                # Receiving CRC
                if b'\x01' == r:
                    # Restart header
                    hdr = b""
                    state = 1
                elif b'\x02' == r:
                    # Restart body
                    body = b""
                    state = 2
                elif b'\x03' == r:
                    # restart CRC
                    crc = b""
                elif b'\x04' == r:
                    # Message complete
                    state = 0

                    if dbug:
                        print("")

                    # Decode the CRC passed in the message
                    msgCrc = int(crc.decode('UTF-8'), 16)

                    # Calc CRC of the received message
                    crcInst = Crc32()
                    crcInst.process(body)
                    calcCrc = crcInst.final()

                    # Compare the CRCs
                    if msgCrc != calcCrc:
                        self._debug(f"msgCrc: {msgCrc:08x}, calcCrc: {calcCrc:08x}", dbug=dbug)
                        return None
                    return hdr.decode("UTF-8"), body.decode("UTF-8")
                elif r.lower() in b"0123456789abcdef":
                    # Store crc character
                    crc += r.lower()
                else:
                    # Invalid character
                    self._debug(f"Invalid CRC character ({r}) received", dbug=dbug)
                    state = 0

        # Timed out
        self._debug("Timed out waiting for response", dbug=dbug)
        return None

    def version(self) -> str:
        '''Return script version'''
        return self._version

    def fail_reason(self) -> str:
        '''Return reason for most recent failure'''
        return self._failReason

    def open(self, dbug:bool=False) -> bool:
        '''open the serial port'''
        if self.port.is_open:
            return True
        print(f"open {self.comm_dev}")
        try:
            self.port.open()
        except Exception as e:
            self._fail(f"Failed to open {self.comm_dev}: {e}", dbug=dbug)
            return False
        self._failReason = ""
        return self.port.is_open

    def close(self) -> None:
        '''close the serial port'''
        if self.port.is_open:
            self.port.close()

    def reset(self) -> bool:
        '''Reset the attached device'''
        if not self.port.is_open:
            return False
        self.port.rts = True
        sleep(0.05)
        self.port.rts = False
        return True

    def command(self, cmd:str, params:dict=None, timeout:float=5.0, dbug:bool=False) -> str|None:
        '''Send a command to unit under test, receive and return the response'''
        if params is None:
            msg = json.dumps({"method": cmd})
        else:
            msg = json.dumps({"method": cmd, "params": params})

        # Apply mutex in case multiple threads are operating
        with self.mutex:
            self.port.reset_input_buffer()
            self._send_mesg("CMD", msg, dbug=dbug)
            resp = self._recv_mesg(timeout=timeout, dbug=dbug)

        #print(f"recvMesg: {resp}")
        if resp is None:
            return None

        # unpack the response tuple
        hdr, body = resp
        if "RESP" == hdr:
            try:
                r = json.loads(body)
            except:
                self._fail("Response not proper JSON", dbug=dbug)
                self._debug(body, dbug=dbug)
                return None
            if 'result' in r:
                return r['result']
            elif 'error' in r:
                e = r['error']
                self._fail(f"Command error - code: {e['code']}, mesg: {e['message']}", dbug=dbug)
                return None
            else:
                self._fail(f"Unexpected data: {body}", dbug=dbug)
                return None
        elif "ERR" == hdr:
            self._fail(f"Remote error: {body}", dbug=dbug)
            return None
        else:
            self._fail(f"Unexpected header: '{hdr}'", dbug=dbug)
            return None

    def command_no_resp(self, cmd:str, params:dict=None, timeout=2.0, dbug:bool=False) -> bool:
        '''Send command and return success/fail status with no data'''
        result = self.command(cmd, params, timeout=timeout, dbug=dbug)
        if result is None:
            return False
        # print(result)
        return 0 == result

    def set_local_baud(self, baud:int) -> bool:
        '''Change the local baud rate'''
        # Linux requires closing and re-opening the port
        if self.system == "Linux":
            self.close()
        self.port.baudrate = baud
        if self.system == "Linux":
            self.open()
        return True

class testerApi(testerComm):
    '''Core API function common to tester host firmware'''
    def __init__(self, port:str, baud:int=115200, timeout:float=0.5) -> None:
        super().__init__(port, baud, timeout)

    def fw_version(self, timeout:float=1.0, dbug:bool=False) -> str|None:
        '''Read and return the firmware version'''
        ret = self.command("version", timeout=timeout, dbug=dbug)
        if ret is None:
            return None
        return ret['version']

    def uptime(self) -> int|None:
        '''Read and return number of seconds uut firmware has been running'''
        ret = self.command("uptime")
        if ret is None:
            return None
        return ret['uptime']

    def reboot(self) -> bool:
        '''Signal the uut to reboot'''
        return self.command_no_resp("reboot")

    def chip_info(self, dbug:bool=False) -> dict|None:
        '''Return information about the uut CPU'''
        return self.command("chip-info", dbug=dbug)

    def echo(self, data:str, dbug:bool=False) -> str|None:
        '''Perform an echo test over the serial link to the UUT'''
        ret = self.command("echo", params={'data':data}, dbug=dbug)
        if ret is None:
            return None
        return ret['data']

    def baud_set(self, baud:int, dbug:bool=False) -> bool:
        '''Change the uut serial baud rate'''
        if not self.command_no_resp("set-baud", params={'value':baud}, dbug=dbug):
            return False
        if dbug:
            self.spy(2)
        sleep(0.25)
        self.set_local_baud(baud)
        return True
