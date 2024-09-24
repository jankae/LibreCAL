# A VNA is needed to measure and create the factory coefficients.
# This file contains the necessary function to extract data from this VNA.
# Please implement all functions according to the VNA you are using.

def checkIfReady() -> bool:
    """
    checkIfReady checks if the VNA is connected and ready to be used
    
    This function can also be used for initiliazing the VNA. It is called only
    once at the beginning
       
    :return: True if VNA is ready. False otherwise
    """   
    return False;

def getPorts() -> int:
    """
    getPorts returns the number of ports the VNA has
    
    Creation of factory coefficients if faster if more ports are available.
    The VNA needs at least 2 ports and at most 4 ports can be utilized.
    
    :return: Number of ports on the VNA
    """
    return 0

def getInfo() -> str:
    """
    getInfo returns information about this VNA (such as model number, serial number)

    :return: Any useful string identifying the VNA
    """
    return ""

def measure():
    """
    measure starts a measurement and returns the S parameter data
    
    This function should start a new measurement and block until the data
    is available. No previous measurements must be returned.
    
    Measurements are returned as a dictionary:
        Key: S parameter name (e.g. "S11")
        Value: List of tuples: (frequency, complex S parameter)
    
    :return: Measurements
    """
    return {}
