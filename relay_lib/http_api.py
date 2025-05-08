from wifi_comm import wifiComm

class httpAPI:
    def __init__(self, url:str, wifi:wifiComm) -> None:
        self.url: str = url
        self.wifi: wifiComm = wifi
        self._fail_reason: str = ""

    def fail_reason(self) -> str:
        '''Return reason for most recent error'''
        return self._fail_reason

    def command(self, code:str, params:dict|None=None, dbug:bool=False) -> dict | None:
        '''Send a command to the target test API and return response'''
        self._fail_reason = ""

        data: dict = {"cmd": code}
        if params is not None:
            # add parameters if provided
            data['params'] = params

        ret = self.wifi.http_post(self.url, data=data, dbug=dbug)
        # print(f"http_post ret: {ret}")
        if ret is None:
            self._fail_reason = "POST attempt failed"
            return None
        
        status: int = ret.get('status_code', 0)
        if status != 200:
            self._fail_reason = f"UUT HTTP error {status}"
            return None

        text: dict = ret['text']
        if text['result'] == 'ok':
            return {"status": 0, "data": text.get('data', None)}

        if text['result'] == 'error':
            if 'reason' in text:
                self._fail_reason = f"UUT returned error: \"{text['reason']}\""
        else:
            self._fail_reason = f"Unrecognized response from UUT: {text}"
        return {"status": -1}

    def command_no_resp(self, cmd:str, params:dict|None=None, dbug:bool=False) -> bool:
        '''Send a command to the target test API and return success/fail status'''
        result = self.command(cmd, params, dbug=dbug)
        # print(result)
        if result is None:
            # fail reason has been set
            return False
        return 0 == result['status']
