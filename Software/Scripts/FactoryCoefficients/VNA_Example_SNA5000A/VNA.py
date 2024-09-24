# A VNA is needed to measure and create the factory coefficients.
# This file contains the necessary function to extract data from this VNA.
# Please implement all functions according to the VNA you are using.

from VNA_Example_SNA5000A.SNA5000A import SNA5000A
import time

vna = None

def checkIfReady() -> bool:
    """
    checkIfReady checks if the VNA is connected and ready to be used
    
    This function can also be used for initiliazing the VNA. It is called only
    once at the beginning
       
    :return: True if VNA is ready. False otherwise
    """
    global vna
    try:
        vna = SNA5000A()
    except:
        print("Failed to detect VNA")
        return False
        
    vna.stop_sweep()
    table = vna.SegmentTable()
    table.add(vna.Segment(9000, 100000, 92))
    table.add(vna.Segment(110000, 1000000, 90))
    table.add(vna.Segment(1100000, 10000000, 90))
    table.add(vna.Segment(11000000, 100000000, 90))
    table.add(vna.Segment(110000000, 8.5e9, 840))
    vna.set_source_power(0)
    vna.set_IF_bandwidth(1000)
    vna.set_segment_table(table)
    vna.set_sweep_type(vna.SweepType.Segment)
    vna.set_excited_ports(range(1, vna.num_ports+1))
    
    return True

def getPorts() -> int:
    """
    getPorts returns the number of ports the VNA has
    
    Creation of factory coefficients if faster if more ports are available.
    The VNA needs at least 2 ports and at most 4 ports can be utilized.
    
    :return: Number of ports on the VNA
    """
    return vna.num_ports

def getInfo() -> str:
    """
    getInfo returns information about this VNA (such as model number, serial number)

    :return: Any useful string identifying the VNA
    """
    return vna.inst.query('*IDN?')

def measure():
    """
    measure starts a measurement and returns the S parameter data
    
    This function should start a new measurement and block until the data
    is available. No previous measurements must be returned.
    
    Measurements are returned as a dictionary:
        Key: S parameter name (e.g. "S11")
        Value: List of tuples: (frequency, complex)
    
    :return: Measurements
    """
    return vna.blocking_single_sweep({"S11","S12","S13","S14","S21","S22","S23","S24","S31","S32","S33","S34","S41","S42","S43","S44"})
