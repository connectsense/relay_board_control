from time import sleep, time
from test_comm import testerApi
from base64 import b64encode

class wifiComm:
    def __init__(self, test_api:testerApi) -> None:
        self.api: testerApi = test_api

    def ble_scan(self, duration:float=10) -> list|None:
        '''The uut will scan for visible BLE devices and return a list'''
        timeout = duration + 5
        return self.api.command("ble-scan", params={"duration": duration}, timeout=timeout)

    def ble_scan_for(self, name:str, duration:float=10) -> list|None:
        '''The uut will scan for BLE devices having names starting with the provided string'''
        timeout = duration + 5
        return self.api.command("ble-scan-for", params={"name": name, "duration": duration}, timeout=timeout)

    def wifi_scan(self, timeout:float=10) -> list|None:
        '''Return list of Wi-Fi access points visible to the uut'''
        return self.api.command("wifi-scan", timeout=timeout)

    def wifi_status(self, dbug:bool=False) -> dict|None:
        '''Return status of Wi-Fi connection between uut and access point'''
        return self.api.command("wifi-status", dbug=dbug)

    def wifi_connect(self, ssid:str, passwd:str|None=None, timeout:float=10, dbug:bool=False) -> bool:
        '''Connect uut to a Wi-Fi access point'''
        params = {"ssid": ssid}
        if passwd is not None:
            params["pass"] = passwd

        if not self.api.command_no_resp("wifi-connect", params=params, dbug=dbug):
            return False

        # if dbug:
        #     # echo any debug messages that may be output from the unit
        #     self.api.spy(5)

        endTime = time() + timeout
        while time() < endTime:
            sleep(0.5)
            st = self.wifi_status(dbug=dbug)
            if st is None:
                continue
            if st['ip_assigned']:
                if dbug:
                    print(f"Connected as {st['ip_addr']}")
                return True
        return False

    def wifi_disconnect(self) -> bool:
        '''Close active connection between uut and access point'''
        return self.api.command_no_resp("wifi-disconnect")

    def http_post(self, url:str, data:str=None, dbug=False) -> dict|None:
        params = {'url': url} if data is None else {'url': url, 'data': data}
        return self.api.command("http-post", params=params, dbug=dbug)

    def http_post_bin(self, url:str, data:bytes) -> dict|None:
        params = {'url': url, 'data': b64encode(data).decode('utf-8')}
        return self.api.command("http-post-bin", params=params)

    def http_get(self, url:str) -> dict|None:
        return self.api.command("http-get", params={'url':url})

    def http_stream_open(self, url:str, method:str, wrLen:int, hdrs:list[dict]|None=None, dbug:bool=False) -> bool:
        '''
        supported methods: 'post'
        wrLen is the total size of the file being sent
        hdrs is a list of dictionaries, each dictionary of the form {'name':'<header name>', 'value':'<header value>'}
        '''
        params = {'url':url, 'method':method, 'wr_len':wrLen}
        if hdrs is not None:
            params['hdr'] = hdrs
        return self.api.command_no_resp("http-open", params=params, dbug=dbug)

    def http_stream_close(self, dbug=False) -> bool:
        return self.api.command_no_resp("http-close", dbug=dbug)

    def http_stream_write_bin(self, data:bytes, dbug=False) -> bool:
        b64_bytes = b64encode(data)
        b64_str = b64_bytes.decode('UTF-8')
        return self.api.command_no_resp("http-write-bin", params={'data':b64_str}, dbug=dbug)

    def http_stream_finish(self, dbug=False) -> dict:
        return self.api.command("http-write-fin", dbug=dbug)

