import pyvisa
from enum import Enum

from bokeh.models import Segment


class SNA5000A:
    # Class to control a siglent SNA5000A vector network analyzer

    def __init__(self, ip="192.168.22.8"):
        rm = pyvisa.ResourceManager('@py')
        dev_addr = "TCPIP0::"+ip+"::INSTR"
        self.inst = rm.open_resource(dev_addr)
        # check ID
        id = self.inst.query('*IDN?')
        if not id.startswith('Siglent Technologies,SNA50'):
            raise RuntimeError("Wrong device ID received:"+id)
        self.num_ports = int(self.read(":SERVICE:PORT:COUNT?"))
        self.excited_ports = self.num_ports

    def set_excited_ports(self, portlist):
        if len(portlist) == 0:
            # disabling all ports is not allowed
            return
        # enable traces for the specified ports
        for i in range(1, self.num_ports+1):
            if i in portlist:
                # enable the trace
                self.write(":DISP:WIND:TRAC" + str(i) + " 1")
                # set the parameter
                self.write(":CALC:PAR" + str(i) + ":DEF S" + str(i) + str(i))
            else:
                # disable the trace
                self.write(":DISP:WIND:TRAC" + str(i) + " 0")
        self.excited_ports = len(portlist)

    def set_start_freq(self, freq):
        self.write(":SENS:FREQ:STAR "+str(freq))

    def set_stop_freq(self, freq):
        self.write(":SENS:FREQ:STOP "+str(freq))

    def set_IF_bandwidth(self, bandwidth):
        self.write(":SENS:BWID "+str(bandwidth))

    def set_source_power(self, power):
        self.write(":SOUR:POW "+str(power))

    def set_points(self, points):
        self.write(":SENS:SWEEP:POINTS "+str(points))

    class SweepType(Enum):
        Linear = "LIN"
        Logarithmic = "LOG"
        Segment = "SEGM"
        Power = "POW"
        CW = "CW"

    def set_sweep_type(self, type: SweepType):
        self.write(":SENS:SWEEP:TYPE "+type.value)

    class Segment:
        def __init__(self, start = 100000, stop = 1000000, points = 21):
            self.start = start
            self.stop = stop
            self.points = points

    class SegmentTable:
        def __init__(self):
            self.segments = []

        def clear(self):
            self.segments = []

        def add(self, segment):
            self.segments.append(segment)

    def set_segment_table(self, table: SegmentTable):
        cmd = ":SENS:SEGM:DATA 5,0,0,0,0,0"
        # Add number of segments
        cmd += ","+str(len(table.segments))
        for s in table.segments:
            cmd += f",{s.start},{s.stop},{s.points}"
        self.write(cmd)

    def get_trace_data(self, parameters: set):
        # example call: get_trace_data({"S11", "S21"})
        xdata = self.read(":CALC:DATA:XAXIS?")
        rawdata = {}
        for p in parameters:
            rawdata[p] = self.read(":SENS:DATA:CORR? "+p)
        # parse received data
        freqList = []
        for val in xdata.split(","):
            freqList.append(float(val))
        parsedData = {}
        for param in rawdata.keys():
            real = []
            imag = []
            doubleValues = rawdata[param].split(",")
            for i in range(0, len(doubleValues), 2):
                real.append(float(doubleValues[i]))
                imag.append(float(doubleValues[i+1]))
            if len(real) != len(freqList):
                raise Exception("X/Y data vectors with different lengths, unable to parse data")
            parsedData[param+"_real"] = real
            parsedData[param+"_imag"] = imag
        ret = {}
        for p in parameters:
            trace = []
            for i in range(len(freqList)):
                freq = freqList[i]
                real = parsedData[p + "_real"][i]
                imag = parsedData[p + "_imag"][i]
                trace.append((freq, complex(real, imag)))
            ret[p] = trace
        return ret

    def start_single_sweep(self):
        self.start_continuous_sweep()
        self.write(":INIT:IMM")

    def blocking_single_sweep(self, data=set()):
        self.write(":TRIG:SOUR MAN")
        self.start_continuous_sweep()

        # Sweep may take a long time, so we need to adjust the timeout of the visa connection
        sweeptime = float(self.read(":SENS:SWE:TIME?"))
        timeout = self.excited_ports * sweeptime + 10
        self.write("TRIG:SING")
        old_timeout = self.inst.timeout
        self.inst.timeout = timeout * 1000
        self.read("*OPC?")
        self.inst.timeout = old_timeout
        ret = self.get_trace_data(data)
        self.write(":TRIG:SOUR INT")
        return ret

    def start_continuous_sweep(self):
        self.write(":INIT:CONT 1")

    def stop_sweep(self):
        self.write(":INIT:CONT 0")

    def sweep_complete(self) -> bool:
        print(self.read(":SENS:AVER:COUN?"))
        return True

    def write(self, command):
        self.inst.write(command)

    def read(self, command):
        return self.inst.query(command)

    def reset(self):
        self.inst.write("*RST")