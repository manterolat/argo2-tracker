'''
Argo 2 Ground Station

Tomas Manterola

Receives data from RFM96W receiver (connected to Arduino/Microcontroller), displays information about capsule, and sends data to HabHub.

Dependencies:
 - crcmod
 - pyserial

'''


import glob
import logging
import re
import sys
import tkFont
import tkMessageBox
import time
import ttk
import urllib
import webbrowser

import Tkinter as tk
from ScrolledText import ScrolledText

# Install missing packages if necessary
try:
    import crcmod.predefined
    import serial

except ImportError:
    try:
        import pip
    except ImportError:
        print("pip must be installed in order to install necessary packages!")
        webbrowser.open("https://pip.pypa.io/en/stable/installing/")
        raw_input("Press any key to exit...")
        quit()
    
    print("Installing necessary packages...")
    pip.main(['install', 'crcmod'])
    pip.main(['install', 'pyserial'])
    raw_input("Press any key to exit...")
    quit()


__version__ = "1.0.0"

SERIAL_PORT_SELECT_ERROR    = ["Serial Port Select Error", "Please select valid serial port from the list."]
SERIAL_PORT_START_ERROR     = ["Serial Port Start Error", "Couldn't open serial port. Make sure the device is connected and that the selected serial port is the correct one."]
SERIAL_PORT_READ_ERROR      = ["Serial Port Read Error", "An error occurred while attempting to read data from the serial port. Make sure the device is connected and that the selected serial port is the correct one.\nRetry, or disconnect?"]
SERIAL_PORT_NOT_CONNECTED   = ["Serial Port Not Connected", "To send a command the serial port must be connected. Please plug in the receiver and select it from the list."]

CALLSIGN_LENGTH_ERROR       = ["Callsign Length Error", "Callsign must have 3 or more characters."]

CONNECTION_ERROR            = ["Connection Error", "An error occurred while sending data to HabHub. Check your Internet connection.\nRetry, or set to offline?"]

HELP_MESSAGE                = ["Help", "This program is designed to be used with a LoRa module and an Arduino connected through a serial connection.\nStart by connecting your receiver to your computer using a USB cable."]
ABOUT_MESSAGE               = ["About", "Argo 2 Ground Station\n\nTool for communicating with Argo 2 transceiver and uploading data to HabHub tracker.\n\nAuthor: Tomas Manterola\nVersion: " + __version__ + "\n"]


GENERAL_COMMANDS            = [
                                ["Set Transmit Power",  "0,POWER",  "Set tracker's transmit power to [POWER].\nValue can range from 5 to 23 dBm.\nWARNING: Low TX power could result in signal from tracker being too weak to receive."],
                                ["Set GPS Nav Mode",    "1,MODE",   "Set Navigation Mode of tracker's GPS to [MODE].\nValue can be either 0 (Pedestrian) or 1 (Airborne < 1G).\nWarning: GPS must be in Airborne mode at altitudes above 9 km to work."],
                                ["Set GPS Power Mode",  "2,MODE",   "Set Power Mode of tracker's GPS to [MODE].\nValue can be either 0 (Max Performance) or 1 (Power Saving)."],
                                ["Enable Buzzer",       "3,1",      "Enable buzzer to produce sound."],
                                ["Disable Buzzer",      "3,0",      "Disable buzzer to stop producing sound.\nWill only work if buzzer has been previously enabled."],
                                ["Set Tracker State",   "4,STATE",  "Set tracker's state to [STATE], which controls settings such as TX Power, Buzzer and GPS Mode.\nValues can be:\n\t0 - Standby (below 500 m)\n\t1 - Rising (above 500 m and rising)\n\t2 - Falling (High) (above 9,000 m and falling)\n\t3 - Falling (Low) (below 9,000 m and falling)\n\t4 - Landing (below 2,000 and falling)"],
                               #["Set Frequency",       "5,FREQ",   "Set tracker's transmit and receive frequency to [FREQ].\nValue can range from 410.000 to 525.000 MHz.\nWARNING: Receiver's frequency must match tracker's frequency in order to receive messages."],
                                ["Custom Message",      "ID,VALUE", "Send custom message to capsule.\nWARNING: Certain messages could have unwanted effects on the tracker."]
]


global app
global callsign
global callsign_temp
global command_desc
global command_raw
global last_command
global last_data_sentence
global logger
global online
global parsed_data
global rssi
global sent_logger
global ser
global serial_port
global serial_port_wait
global tx_power

global callsign_textbox
global command_listbox
global connect_button
global crc_label
global data_textbox

ser = serial.Serial()
serial_port_wait = 1000



class StatusWindow(tk.Toplevel):
    
    def __init__(self):
        global crc_label
        global parsed_data
        global rssi
        
        # Create Window
        tk.Toplevel.__init__(self)
        self.title("Status")
        self.geometry("210x530+100+100")
        
        
        # Add UI components
        
        # Bigger Font
        big_font = tkFont.Font(size=10)
        
        
        # Frames
        general_frame   = tk.LabelFrame(self, text="General")
        tracking_frame  = tk.LabelFrame(self, text="Tracking")
        sensors_frame   = tk.LabelFrame(self, text="Sensors")
        status_frame    = tk.LabelFrame(self, text="Status")
        self.columnconfigure(0, weight=1) # Make all frames resizeable
        
        
        # General Frame
        callsign_label  = tk.Label(general_frame, font=big_font, textvariable=parsed_data[0][1])
        sentence_label  = tk.Label(general_frame, font=big_font, textvariable=parsed_data[1][1])
        time_label      = tk.Label(general_frame, font=big_font, textvariable=parsed_data[2][1])
        
        tk.Label(general_frame, font=big_font, text="Callsign:").grid(row=0, column=0, sticky='w')
        tk.Label(general_frame, font=big_font, text="Sentence #:").grid(row=1, column=0, sticky='w')
        tk.Label(general_frame, font=big_font, text="Time:").grid(row=2, column=0, sticky='w')
        
        callsign_label.grid(row=0, column=1, sticky='e')
        sentence_label.grid(row=1, column=1, sticky='e')
        time_label.grid(row=2, column=1, sticky='e')
        
        general_frame.columnconfigure(1, weight=1)
        general_frame.grid(row=0, column=0, sticky='nws', padx=(5, 5), pady=(5, 5))
        
        
        # Tracking Frame
        latitude_label  = tk.Label(tracking_frame, font=big_font, textvariable=parsed_data[3][1])
        longitude_label = tk.Label(tracking_frame, font=big_font, textvariable=parsed_data[4][1])
        altitude_label  = tk.Label(tracking_frame, font=big_font, textvariable=parsed_data[5][1])
        v_speed_label   = tk.Label(tracking_frame, font=big_font, textvariable=parsed_data[6][1])
        speed_label     = tk.Label(tracking_frame, font=big_font, textvariable=parsed_data[7][1])
        course_label    = tk.Label(tracking_frame, font=big_font, textvariable=parsed_data[8][1])
        
        tk.Label(tracking_frame, font=big_font, text="Latitude:").grid(row=0, column=0, sticky='w')
        tk.Label(tracking_frame, font=big_font, text="Longitude:").grid(row=1, column=0, sticky='w')
        tk.Label(tracking_frame, font=big_font, text="Altitude:").grid(row=2, column=0, sticky='w')
        tk.Label(tracking_frame, font=big_font, text="Vertical Speed:").grid(row=3, column=0, sticky='w')
        tk.Label(tracking_frame, font=big_font, text="Horizontal Speed:").grid(row=4, column=0, sticky='w')
        tk.Label(tracking_frame, font=big_font, text="Course:").grid(row=5, column=0, sticky='w')
        
        latitude_label.grid(row=0, column=1, sticky='e', columnspan=2)
        longitude_label.grid(row=1, column=1, sticky='e', columnspan=2)
        altitude_label.grid(row=2, column=1, sticky='e')
        v_speed_label.grid(row=3, column=1, sticky='e')
        speed_label.grid(row=4, column=1, sticky='e')
        course_label.grid(row=5, column=1, sticky='e')
        
        tk.Label(tracking_frame, font=big_font, text=parsed_data[5][2]).grid(row=2, column=2, sticky='e')
        tk.Label(tracking_frame, font=big_font, text=parsed_data[6][2]).grid(row=3, column=2, sticky='e')
        tk.Label(tracking_frame, font=big_font, text=parsed_data[7][2]).grid(row=4, column=2, sticky='e')
        tk.Label(tracking_frame, font=big_font, text=parsed_data[8][2]).grid(row=5, column=2, sticky='e')
        
        tracking_frame.columnconfigure(1, weight=1)
        tracking_frame.columnconfigure(2, weight=1)
        tracking_frame.grid(row=1, column=0, sticky='nesw', padx=(5, 5), pady=(5, 5))
         
        
        # Sensors Frame
        ext_temp_label  = tk.Label(sensors_frame, font=big_font, textvariable=parsed_data[9][1])
        int_temp_label  = tk.Label(sensors_frame, font=big_font, textvariable=parsed_data[10][1])
        pressure_label  = tk.Label(sensors_frame, font=big_font, textvariable=parsed_data[11][1])
        humidity_label  = tk.Label(sensors_frame, font=big_font, textvariable=parsed_data[12][1])
        
        tk.Label(sensors_frame, font=big_font, text="Ext. Temp:").grid(row=0, column=0, sticky='w')
        tk.Label(sensors_frame, font=big_font, text="Int. Temp:").grid(row=1, column=0, sticky='w')
        tk.Label(sensors_frame, font=big_font, text="Pressure:").grid(row=2, column=0, sticky='w')
        tk.Label(sensors_frame, font=big_font, text="Humidity:").grid(row=3, column=0, sticky='w')
        
        ext_temp_label.grid(row=0, column=1, sticky='e')
        int_temp_label.grid(row=1, column=1, sticky='e')
        pressure_label.grid(row=2, column=1, sticky='e')
        humidity_label.grid(row=3, column=1, sticky='e')
        
        tk.Label(sensors_frame, font=big_font, text=parsed_data[9][2]).grid(row=0, column=2, sticky='e')
        tk.Label(sensors_frame, font=big_font, text=parsed_data[10][2]).grid(row=1, column=2, sticky='e')
        tk.Label(sensors_frame, font=big_font, text=parsed_data[11][2]).grid(row=2, column=2, sticky='e')
        tk.Label(sensors_frame, font=big_font, text=parsed_data[12][2]).grid(row=3, column=2, sticky='e')
        
        sensors_frame.columnconfigure(1, weight=1)
        sensors_frame.columnconfigure(2, weight=1)
        sensors_frame.grid(row=2, column=0, sticky='news', padx=(5, 5), pady=(5, 5))
        
        
        # Status Frame
        v_bat_label     = tk.Label(status_frame, font=big_font, textvariable=parsed_data[13][1])
        sat_num_label   = tk.Label(status_frame, font=big_font, textvariable=parsed_data[14][1])
        status_label    = tk.Label(status_frame, font=big_font, textvariable=parsed_data[15][1])
        ack_label       = tk.Label(status_frame, font=big_font, textvariable=parsed_data[16][1])
        crc_label       = tk.Label(status_frame, font=big_font, textvariable=parsed_data[17][1])
        rssi_label      = tk.Label(status_frame, font=big_font, textvariable=rssi)
        
        tk.Label(status_frame, font=big_font, text="Batt. Voltage:").grid(row=0, column=0, sticky='w') 
        tk.Label(status_frame, font=big_font, text="Satellite #:").grid(row=1, column=0, sticky='w')
        tk.Label(status_frame, font=big_font, text="Status:").grid(row=2, column=0, sticky='w')
        tk.Label(status_frame, font=big_font, text="ACK:").grid(row=3, column=0, sticky='w')
        tk.Label(status_frame, font=big_font, text="Checksum:").grid(row=4, column=0, sticky='w')
        tk.Label(status_frame, font=big_font, text="RSSI:").grid(row=5, column=0, sticky='w')
        
        v_bat_label.grid(row=0, column=1, sticky='e')
        sat_num_label.grid(row=1, column=1, sticky='e')
        status_label.grid(row=2, column=1, sticky='e')
        ack_label.grid(row=3, column=1, sticky='e')
        crc_label.grid(row=4, column=1, sticky='e')
        rssi_label.grid(row=5, column=1, sticky='e')
        
        tk.Label(status_frame, font=big_font, text=parsed_data[13][2]).grid(row=0, column=2, sticky='w')
        tk.Label(status_frame, font=big_font, text="dBm").grid(row=5, column=2, sticky='w')
        
        status_frame.columnconfigure(2, weight=1)
        status_frame.grid(row=3, column=0, sticky='nws', padx=(5, 5), pady=(5, 5))
        
        

class MainApplication(tk.Frame):
    
    def __init__(self, parent):
        global online
        global callsign
        global callsign_temp
        global command_desc
        global command_raw
        global last_command
        global logger
        global parsed_data
        global tx_power
        
        global serial_port
        global serial_port_options
        global port_list
        
        global callsign_entry
        global command_listbox
        global connect_button
        global data_textbox
        
        # Create main Frame
        tk.Frame.__init__(self, parent)
        self.parent = parent
        self.parent.title("Argo 2 Ground Station")
        
        
        # Add UI components
        
        # Menu Bar
        self.menu_bar = tk.Menu(self)
        self.parent.config(menu=self.menu_bar)
        
        # File Menu
        self.file_menu = tk.Menu(self.menu_bar)
        self.file_menu.add_command(label="Exit", underline=0, command=on_exit)
        
        # Receiver Menu
        self.capsule_menu = tk.Menu(self.menu_bar)
        self.capsule_menu.add_command(label="Capsule Status", underline=0, command=show_status_window)
        self.capsule_menu.add_separator()
        self.capsule_menu.add_command(label="Send Command", underline=0, command=send_command)
        
        # HabHub Menu
        self.tracking_menu = tk.Menu(self.menu_bar)
        self.tracking_menu.add_command(label="Google Maps", underline=0, command=lambda : webbrowser.open("http://google.com/maps/place/" + parsed_data[3][1].get() + "," + parsed_data[4][1].get()))
        self.tracking_menu.add_command(label="HabHub", underline=0, command=lambda : webbrowser.open("http://tracker.habhub.org/"))
        self.tracking_menu.add_separator()
        self.tracking_menu.add_checkbutton(label="Online", underline=0, offvalue=0, onvalue=1, variable=online)
        
        # Help Menu
        self.help_menu = tk.Menu(self.menu_bar)
        self.help_menu.add_command(label="Help", underline=0, command=self.show_help)
        self.help_menu.add_command(label="About", underline=0, command=self.show_about)
        
        # Add Menus to Menu Bar
        self.menu_bar.add_cascade(label="File", underline=0, menu=self.file_menu)
        self.menu_bar.add_cascade(label="Capsule", underline=0, menu=self.capsule_menu)
        self.menu_bar.add_cascade(label="Tracking", underline=2, menu=self.tracking_menu)
        self.menu_bar.add_cascade(label="Help", underline=0, menu=self.help_menu)
        
        
        # Frames
        left_frame = tk.Frame(self.master)
        
        
        # Serial Port Label
        tk.Label(left_frame, text="Port:").grid(row=0, column=0)
        
        # Serial Port OptionMenu - allow user to choose serial port to connect to
        serial_port = tk.StringVar(self.master)
        port_list = ("", "")
        serial_port_options = tk.OptionMenu(left_frame, serial_port, *list(port_list))
        serial_port_options.config(width=6)
        serial_port_options.bind('<Button-1>', update_serial_list)
        serial_port_options.grid(row=0, column=1, sticky='w', padx=(10, 0))
        
        # Serial Port Connect Button
        connect_button = tk.Button(left_frame, text="Connect", command=connect_serial)
        connect_button.grid(row=1, column=1, columnspan=2, sticky='w', padx=12, pady=(0,30))
                
        
        # Callsign Label
        tk.Label(left_frame, text="Callsign:").grid(row=2, column=0, sticky='e')
        
        # Callsign Entry
        callsign_temp = tk.StringVar(self.master)
        callsign_temp.set("PAN1")
        callsign_entry = tk.Entry(left_frame, width=12, textvariable=callsign_temp)
        callsign_entry.grid(row=2, column=1)
        
        # Callsign Set Button
        callsign = tk.StringVar(self.master)
        callsign.set("PAN1")
        callsign_button = tk.Button(left_frame, text="Set callsign", command=set_callsign)
        callsign_button.grid(row=3, column=1, columnspan=2, sticky='w', padx=(7,0))
        
        
        # Online Checkbox - allows user to decide whether to send data to HabHub or not
        self.online_checkbutton = tk.Checkbutton(left_frame, text="Offline", fg="red", variable=online)
        self.online_checkbutton.grid(row=4, column=1, columnspan=2, sticky='w', padx=(5, 0), pady=(20, 0))
        
        
        # Data Text Box - data recieved from capsule
        data_textbox = ScrolledText(self.master, relief='sunken')
        data_textbox.configure(state=tk.DISABLED, width=64, height=16, wrap=tk.NONE)  # Make data uneditable
        data_textbox.grid(row=0, column=2, columnspan=5, padx=(20, 0), pady=(10, 10))
        
        #data_scrollbar = tk.Scrollbar(self.master, command=data_textbox.yview)
        #data_scrollbar.grid()
        
        
        
        left_frame.columnconfigure(0, pad=10)
        left_frame.rowconfigure(0, pad=10)
        left_frame.rowconfigure(1, pad=10)
        left_frame.rowconfigure(2, pad=10)
        left_frame.rowconfigure(3, pad=10)
        left_frame.grid(row=0, column=0)
        
        
        # Command Listbox
        command_listbox = tk.Listbox(self.master)
        
        for cmd_num in xrange(0, len(GENERAL_COMMANDS)):
            command_listbox.insert(cmd_num, GENERAL_COMMANDS[cmd_num][0])
        
        command_listbox.bind('<<ListboxSelect>>', on_select_command)
        command_listbox.grid(row=1, column=0, rowspan=4, sticky='n')
        
        
        # Command Entry
        command_raw = tk.StringVar()
        command_entry = tk.Entry(self.master, width=20, textvariable=command_raw)
        command_entry.grid(row=1, column=2, padx=(20, 0), pady=(3, 0), sticky='nw')
        
        
        # Command Description
        command_desc = tk.StringVar()
        command_desc_label = tk.Label(self.master, textvariable=command_desc, justify=tk.LEFT)
        command_desc_label.grid(row=2, column=2, columnspan=5, padx=(20, 0), sticky='nw')
        
        
        # Command TX Power
        tk.Label(self.master, text="TX Power:").grid(row=1, column=4, sticky='ne', pady=(3, 0))
        
        tx_power = tk.StringVar()
        tx_power.set(20)
        command_power_option = ttk.Combobox(self.master, width=5, state='readonly', textvariable=tx_power)
        command_power_option['values'] = list(reversed(range(5, 23 + 1)))
        command_power_option.grid(row=1, column=5, sticky='ne', pady=(3, 0))
        
        
        # Command Buttons (Send Command and Last Command)
        last_command = ""
        last_command_button = tk.Button(self.master, text="Load Last Command", command=lambda : command_raw.set(last_command))
        last_command_button.grid(row=1, column=3, sticky='nw')
        
        send_command_button = tk.Button(self.master, text="Send Command", command=send_command)
        send_command_button.grid(row=1, column=6, sticky='ne')
        
        
        # Status Button (show Status Window)
        status_button = tk.Button(self.master, text="Show Status Window", command=show_status_window)
        status_button.grid(row=4, column=6, sticky='se')
        
        
        # Setup serial check in 100 ms (to repeat forever)
        self.after(100, get_serial_data)
        
        
    # Show information about author and program
    def show_about(self):
        tkMessageBox.showinfo(title=ABOUT_MESSAGE[0], message=ABOUT_MESSAGE[1])
        
        
    # Show information on usage and further resources
    def show_help(self):
        tkMessageBox.showinfo(title=HELP_MESSAGE[0], message=HELP_MESSAGE[1])
        
        

# When user selects command from list, enter command into entry and edit command label
def on_select_command(*args):
    global command_desc
    global command_raw
    
    global command_listbox
    
    index = int(command_listbox.curselection()[0])
    
    command_raw.set(GENERAL_COMMANDS[index][1])
    command_desc.set(GENERAL_COMMANDS[index][2])


# Establish serial connection with port selected in 'serial_port'
def connect_serial(*args):
    global ser
    global serial_port
    close_serial()
    
    if serial_port.get():
        write_log(logging.INFO, "Connecting to serial port " + serial_port.get() + "...")
        try: 
            ser = serial.Serial(serial_port.get(), timeout=None)
            write_log(logging.INFO, "Connected!")
            return
        except:
            write_log(logging.ERROR, "Error while connecting to port")
            tkMessageBox.showerror(title=SERIAL_PORT_START_ERROR[0], message=SERIAL_PORT_START_ERROR[1])
    
    else:
        tkMessageBox.showerror(title=SERIAL_PORT_SELECT_ERROR[0], message=SERIAL_PORT_SELECT_ERROR[1])
        
        
# Get a list of all available serial ports
def get_serial_ports(*args):
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # Excludes current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result


# Open status window showing current data from capsule
def show_status_window(*args):
    StatusWindow()


# Send command to capsule (through transceiver)
def send_command(*args):
    global logger
    
    global command_raw
    global last_command
    global ser
    global tx_power
    
    # Do nothing if command is empty
    if not command_raw.get():
        return
    
    # Let user know if not connected to receiver
    if not ser.is_open:
        tkMessageBox.showerror(SERIAL_PORT_NOT_CONNECTED[0], SERIAL_PORT_NOT_CONNECTED[1])
        return
        
    if not tkMessageBox.askokcancel("Confirm Send Command", "Are you sure you want to send this command?\nA badly made message could have unwanted effects."):
        return
    
    last_command = command_raw.get()
    
    write_log(logging.INFO, "Sending command '" + command_raw.get() + "' at " + tx_power.get() + " dBm")
    
    try:
        ser.write(tx_power.get() + ";" + command_raw.get() + "\n")
        
    except:
        write_log(logging.ERROR, "Error sending command!")
    
    


# Update serial port options with list of available serial ports 
def update_serial_list(*args):
    global logger
    global serial_port
    global serial_port_options
    global port_list
    
    serial_port.set("")
    port_list = list(get_serial_ports())
    serial_port_options['menu'].delete(0, 'end')
    
    # Add empty entry (aesthetics)
    serial_port_options['menu'].add_command(label="", command=tk._setit(serial_port, ""))
    
    # Add available ports to OptionMenu list
    for port in port_list:
        serial_port_options['menu'].add_command(label=port, command=tk._setit(serial_port, port))
    
    logger.info("Updated port list")


# Get data from receiver connected through serial port (and save last valid sentence)
# Sentence from receiver has the following format (with sentence from capsule and RSSI of receiver):
# [SENTENCE];[RSSI]
def get_serial_data(*args):
    global app
    global data_textbox
    global last_data_sentence
    global logger
    global ser
    
    data = ""
    if ser.is_open:
        try:
            data = ser.readline(ser.inWaiting())
        except:
            logger.error("Error while attempting to read serial port data")
            if not tkMessageBox.askretrycancel(title=SERIAL_PORT_READ_ERROR[0], message=SERIAL_PORT_READ_ERROR[1], icon="error"):
                close_serial()
    
    if data:
        last_data_sentence = data
        parse_data()
        
        # Send data to HabHub tracker (if online and data is valid)
        if online.get() and len(re.split(',|\*', data.split(";")[0] )) == len(parsed_data):
            send_data(data.split(";")[0]);
    
    
    # For some reason, pySerial needs an entire second to figure out if there is new data in the buffer...
    # Very annoying and time consuming...
    app.after(1000, get_serial_data)


# Update stored data from last data sentence
def parse_data(*args):
    global last_data_sentence
    global parsed_data
    global sent_logger
    
    # Grab and remove RSSI from data string
    try:
        rssi.set(last_data_sentence.split(";")[1].rstrip())
        last_data_sentence = last_data_sentence.split(";")[0]
    except:
        write_log(logging.INFO, "Received data: '" + last_data_sentence.strip("\n") + "'")
        write_log(logging.INFO, " -> Message length: " + str(len(last_data_sentence)))
        write_log(logging.INFO, " -> Wrong message format!")
        return
    
    write_log(logging.INFO, "Received data: '" + last_data_sentence.strip("\n") + "'")
    write_log(logging.INFO, " -> RSSI: " + rssi.get() + " dBm")
    write_log(logging.INFO, " -> Message length: " + str(len(last_data_sentence)))
    
    data = re.split(',|\*', last_data_sentence)
    
    
    try:
        if len(data) != len(parsed_data): raise ValueError("Wrong message format!")
        
        for x in xrange(0, len(data)):
            parsed_data[x][1].set(data[x])
        
        sent_logger.info(last_data_sentence)
        
    except:
        write_log(logging.INFO, " -> Wrong message format!")
        
        # Try to get checksum
        if (len(last_data_sentence.split("*")) == 2):
            parsed_data[17][1].set(last_data_sentence.split("*")[1])
        else:
            write_log(logging.INFO, " -> No checksum found")


# Calculate ccrc16-ccitt checksum
def calc_crc(data):
    ccitt = crcmod.predefined.Crc('crc-ccitt-false')
    ccitt.update(data)
    return str(format(ccitt.crcValue, 'x')).zfill(4)


# Check crc16-ccitt checksum
def check_crc(*args):
        global crc_label
        global logger
        global parsed_data
        global last_data_sentence
        
        check_sum = calc_crc(last_data_sentence.split('*')[0]).upper()
        
        if check_sum == parsed_data[17][1].get().upper():
            write_log(logging.INFO, " -> Correct Checksum: " + check_sum)
            try:
                crc_label.config(fg='dark green')
            except:
                return
                
        else:
            write_log(logging.INFO, " -> Incorrect Checksum: Recv = " + parsed_data[17][1].get() + ", Calc = " + check_sum)
            
            try:
                crc_label.config(fg='red')
            except:
                return


# Set callsign to value in callsign_temp
def set_callsign(*args):
    global callsign
    global callsign_temp
    
    if len(callsign_temp.get()) > 3:
        callsign.set(callsign_temp.get())
        write_log(logging.INFO, "Set callsign to: " + callsign.get())
    else:
        tkMessageBox.showerror(title=CALLSIGN_LENGTH_ERROR[0], message=CALLSIGN_LENGTH_ERROR[1])


# Send data to HabHub tracker
def send_data(*args):
    global callsign
    global last_data_sentence
    global logger
    
    write_log(logging.INFO, "Sending data... ")
    try:
        resp = ""
        while "OK" not in resp:
            url = 'http://habitat.habhub.org/transition/payload_telemetry'
            params = "callsign=" + callsign.get() + "&string=%24%24" + last_data_sentence + "\n&string_type=ascii&metadata={}"
            f = urllib.urlopen(url, params)
            resp = f.read()
            logger.info("Response: " + resp)
            time.sleep(0.5)
        
        write_log(logging.INFO, "Sent Data!")
        
        
    except:
        write_log(logging.ERROR, "Error sending data")
        if tkMessageBox.askretrycancel(title=CONNECTION_ERROR[0], message=CONNECTION_ERROR[1]):
            send_data()
        else:
            online = False
                
                
# Toggle whether data is send to HabHub or not (and change button text/color)
def toggle_online(*args):
    global app
    global logger
    global online
    
    # Going online
    if online.get():
        write_log(logging.INFO, "Going online")
        app.online_checkbutton.config(text="Online", fg="dark green")
            
    # Going offline
    else:
        write_log(logging.INFO, "Going offline")
        app.online_checkbutton.config(text="Offline", fg="red")
        
        
# Write given text to 'data_textbox'
def write_textbox(text):
    global data_textbox
    data_textbox.config(state=tk.NORMAL)
    data_textbox.insert(tk.END, text + "\n")
    data_textbox.config(state=tk.DISABLED)
    scroll_bottom()
    

# Write given text to both 'data_textbox' and logger
def write_log(level, text):
    global logger
    
    logger.log(level, text)
    write_textbox(text)
    
    
# Scroll to bottom of textbox
def scroll_bottom(*args):
    global data_textbox
    data_textbox.see(tk.END)


# Close serial if it is open
def close_serial(*args):
    global ser
    
    if ser.is_open:
        write_log(logging.INFO, "Closing Serial Port")
        ser.close()


# On exit ask the user to confirm and close the serial port before quitting
def on_exit(*args):
    global app
    global logger
    
    if tkMessageBox.askokcancel("Quit", "Are you sure you want to exit?"):
        close_serial()
        logger.info("Quitting...")
        app.quit()


# Start program
def main():
    global app
    global last_data_sentence
    global logger
    global online
    global parsed_data
    global rssi
    global sent_logger
    
    # Start and configure logging
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)
    
    file_handler = logging.FileHandler('GroundStation.log')
    file_handler.setLevel(logging.INFO)
    
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(logging.INFO)
    
    log_formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s")
    file_handler.setFormatter(log_formatter)
    console_handler.setFormatter(log_formatter)
    
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)
    
    
    # Start and configure sentence logging
    sent_logger = logging.getLogger('sentence')
    sent_logger.setLevel(logging.INFO)
    
    file_handler2 = logging.FileHandler('sentences.log')
    file_handler2.setLevel(logging.INFO)
    
    log_formatter2 = logging.Formatter("%(message)s")
    file_handler2.setFormatter(log_formatter2)
    
    sent_logger.addHandler(file_handler2)
    
    
    logger.info("Starting Argo 2 Ground Station")
    
    
    # Initialize window
    root = tk.Tk()
    #root.columnconfigure(0, weight=1)
    #root.rowconfigure(0, weight=1)
    
    
    # Initialize variables
    online = tk.IntVar()
    
    # List containing parsed data from the capsule/receiver in form (name, value, unit)
    # These are saved as 'StringVar' so that widgets update automatically when these are changed
    # Example: ARGO2,10000,22:22:22,-92.1232322,-90.2322323,30000,-12.0,12.0,32.4,-20.25,-10.1,300.0,56.4,4.32,10,5,1*b762
    parsed_data = [
                   ["callsign", tk.StringVar(), ""],        # 0.  Name of Capsule
                   ["sent_id", tk.StringVar(), ""],         # 1.  Sentence ID (number)    
                   ["time", tk.StringVar(), ""],            # 2.  Time (hh:mm:ss)
                   ["latitude", tk.StringVar(), ""],        # 3.  Latitude (decimal)
                   ["longitude", tk.StringVar(), ""],       # 4.  Longitude (decimal)
                   ["altitude", tk.StringVar(), "m"],       # 5.  Altitude (meters)
                   ["v_speed", tk.StringVar(), "m/s"],      # 6.  Vertical speed (meters per second)
                   ["speed", tk.StringVar(), "m/s"],        # 7.  Speed (meters per second)
                   ["course", tk.StringVar(), "deg"],       # 8.  Course (degrees)
                   ["ext_temp", tk.StringVar(), "C"],       # 9.  External temperature (Celsius)
                   ["int_temp", tk.StringVar(), "C"],       # 10. Internal temperature (Celsius)
                   ["pressure", tk.StringVar(), "hPa"],     # 11. Pressure (hPa)
                   ["humidity", tk.StringVar(), "%"],       # 12. Humidity (percent)
                   ["v_bat", tk.StringVar(), "V"],          # 13. Battery voltage (volts)
                   ["sat_num", tk.StringVar(), ""],         # 14  Satellite Number (#)
                   ["status", tk.StringVar(), ""],          # 15. Status/Mode of Capsule (0: stby, 1: >0m, 2: >5,000m, 3: >10,000m, 4: >20,000m, 5: descending, 6: <3,000m descending)
                   ["ACK", tk.StringVar(), ""],             # 16. Acknowledge Message Received (0: no messages received, 1: message received)
                   ["crc", tk.StringVar(), ""]              # 17. Checksum (crc16-ccitt - 4 characters)
    ]
    
    rssi = tk.StringVar()                                   # RSSI: Signal Strength noted by receiver (dBm - Formula: -137 + dBm)
    
    
    # Initialize main window
    app = MainApplication(root)
    
    
    # Setup main window
    root.geometry("720x460+100+100")
    
    try:
        root.iconbitmap("icon.ico")
    except:
        logging.warning("Couldn't find icon file")
    
    
    # Setup bindings/protocols/callbacks
    root.protocol("WM_DELETE_WINDOW", on_exit)  # Run 'on_exit()' when user clicks "Close" button
    online.trace("w", toggle_online)            # Run 'toggle_online()' when value of 'online' changes
    parsed_data[17][1].trace('w', check_crc)
    
    
    # Start main update/window loop
    root.mainloop()
    
    
    
if __name__ == '__main__':
    main()
