#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <TheThingsNetwork.h>
#include <SoftPWM.h>

#include "editline.h"
#include "cmdproc.h"

#define SEND_INTERVAL_MS    10000

#define PIN_LED_RED     12      // PD6
#define PIN_LED_GREEN   6       // PD7
#define PIN_LED_BLUE    11      // PB7
#define PIN_LORA_LED    8       // PB4 xx
#define PIN_ROTARY_SW0  5       // PC6
#define PIN_ROTARY_SW1  13      // PC7
#define PIN_ROTARY_SW2  9       // PB5
#define PIN_ROTARY_SW3  30      // PD5
#define PIN_BUTTON      7       // PE6

#define USB_CABLE_CONNECTED (USBSTA&(1<<VBUS))

//Define both serial interfaces for easier use
#define loraSerial			Serial1
#define usbserial			Serial  //for commisioning and debugging

//Global variables
static TheThingsNetwork ttn(loraSerial, usbserial, TTN_FP_EU868);
static bool ttn_ok = true;

static char line[120];

// printf style formatting to the debug port, maximum length of line is 128
static int print(const char *fmt, ...)
{
    static char line[128];
    va_list ap;

    va_start(ap, fmt);
    int result = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    Serial.write(line);
    return result;
}

static void set_led(int pin, int value)
{
    if (value == 0) {
        SoftPWMEnd(pin);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    } else {
        SoftPWMSet(pin, 255 - value);
    }
}

//! \brief Turn RGB led on with a given state colour of turn it off
static void set_rgb_led(uint8_t r, uint8_t g, uint8_t b)
{
    set_led(PIN_LED_RED, r);
    set_led(PIN_LED_GREEN, g);
    set_led(PIN_LED_BLUE, b);
}

static void onMessage(const uint8_t *payload, size_t size, port_t port)
{
    print("Received %d bytes on port %d\n", size, port);
    if (size == 3) {
        set_rgb_led(payload[0], payload[1], payload[2]);
    }
}

void setup(void)
{
    //Attach USB. Necessary for the serial monitor to work
#if defined(USBCON)
    USBDevice.attach();
    USBCON |= (1 << OTGPADE);   //enables VBUS pad for detection of USB cable
#endif

    // configure rotary pins as input
    pinMode(PIN_ROTARY_SW0, INPUT);
    pinMode(PIN_ROTARY_SW1, INPUT);
    pinMode(PIN_ROTARY_SW2, INPUT);
    pinMode(PIN_ROTARY_SW3, INPUT);

    // set leds as output and turn off (active low)
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LORA_LED, OUTPUT);
    digitalWrite(PIN_LED_RED, 1);
    digitalWrite(PIN_LED_GREEN, 1);
    digitalWrite(PIN_LED_BLUE, 1);
    digitalWrite(PIN_LORA_LED, 1);
    SoftPWMBegin();

    //set the analog reference to the internal reference
    analogReference(INTERNAL);

    //Start serial ports
    loraSerial.begin(57600);    //RN2483 needs 57600 baud rate  
    usbserial.begin(115200);

    EditInit(line, sizeof(line));

    // yellow while initializing
    set_rgb_led(100, 100, 0);

    usbserial.println(F("--- Resetting RN module"));
    ttn.reset(false);
    ttn.onMessage(onMessage);

    // attempt to resume from previously set session
    ttn_ok = ttn.personalize();

    // led off when initialized OK, red when error
    if (ttn_ok) {
        set_rgb_led(0, 0, 0);
    } else {
        set_rgb_led(100, 0, 0);
    }
}

//! \brief Return current value (0-7) from the rotary switch.
static int get_rotary_value(void)
{
    static int pins[] = { PIN_ROTARY_SW0, PIN_ROTARY_SW1, PIN_ROTARY_SW2, PIN_ROTARY_SW3 };

    int val = 0;
    for (int i = 0; i < 3; i++) {
        if (digitalRead(pins[i])) {
            val |= (1 << i);
        }
    }
    return val;
}

static bool get_button_value(void)
{
    return digitalRead(PIN_BUTTON) == 0;
}

//! \brief Turn LoRa led on (true) or off (false)
static void set_lora_led(bool state)
{
    digitalWrite(PIN_LORA_LED, state ? 0 : 1);
}

static void show_help(const cmd_t * cmds)
{
    for (const cmd_t * cmd = cmds; cmd->cmd != NULL; cmd++) {
        print("%10s: %s\n", cmd->name, cmd->help);
    }
}

static int do_led(int argc, char *argv[])
{
    if (argc == 2) {
        // assume hex encoded color
        uint32_t rgb = strtoul(argv[1], NULL, 16);
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >> 8) & 0xFF;
        uint8_t b = (rgb >> 0) & 0xFF;
        set_rgb_led(r, g, b);
        print("set_rgb_led(%d,%d,%d)\n", r, g, b);
        return CMD_OK;
    }
    if (argc == 4) {
        int r = atoi(argv[1]);
        int g = atoi(argv[2]);
        int b = atoi(argv[3]);
        print("set_rgb_led(%d,%d,%d)\n", r, g, b);
        set_rgb_led(r, g, b);
        return CMD_OK;
    }
    return CMD_PARAMS;
}

static int do_lora(int argc, char *argv[])
{
    if (argc < 2) {
        return CMD_PARAMS;
    }
    bool on = (strcmp("0", argv[1]) != 0);
    set_lora_led(on);
    return CMD_OK;
}

static int do_ttn_reset(int argc, char *argv[])
{
    ttn.reset(false);
    return 0;
}

static int do_ttn_status(int argc, char *argv[])
{
    ttn.showStatus();
    return 0;
}

static int do_ttn_join(int argc, char *argv[])
{
    print("OTAA join...\n");
    return ttn.join(1) ? CMD_OK : -1;
}

static int do_ttn_abp(int argc, char *argv[])
{
    bool result;
    if (argc == 4) {
        print("ABP setup (devadr=%s, network key=%s, session key=%s)...\n", argv[1], argv[2],
              argv[3]);
        result = ttn.personalize(argv[1], argv[2], argv[3]);
        if (result) {
            ttn.saveState();
        }
    } else {
        print("ABP setup ()...");
        result = ttn.personalize();
    }
    return result ? CMD_OK : -1;
}

static int do_ttn_otaa(int argc, char *argv[])
{
    if (argc < 3) {
        return CMD_PARAMS;
    }
    print("OTAA setup (appeui=%s, appkey=%s)...\n", argv[1], argv[2]);
    bool result = ttn.provision(argv[1], argv[2]);
    return result ? CMD_OK : -1;
}

static int do_ttn_send(int argc, char *argv[])
{
    int port = 1;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    bool confirm = false;
    if (argc > 2) {
        confirm = (atoi(argv[2]) != 0);
    }
    int sf = 7;
    if (argc > 3) {
        sf = atoi(argv[3]);
    }

    uint8_t rot = get_rotary_value();
    uint8_t payload[1] = {rot};
    ttn_response_t response = ttn.sendBytes(payload, sizeof(payload), port, confirm, sf);
    return (response == TTN_SUCCESSFUL_TRANSMISSION) || (response == TTN_SUCCESSFUL_RECEIVE) ? CMD_OK : -1;
}

// ttn subcommands
static const cmd_t ttn_commands[] = {
    { "reset", do_ttn_reset, "Resets the RN2483" },
    { "status", do_ttn_status, "Shows RN2483 status" },
    { "join", do_ttn_join, "Try to join" },
    { "abp", do_ttn_abp, "[<devadr> <nwkkey> <seskey>] Perform ABP" },
    { "otaa", do_ttn_otaa, "<appeui> <appkey> Perform OTAA" },
    { "send", do_ttn_send, "[port] [confirm] [sf] Send 1-byte data" },
    { NULL, NULL, NULL }
};

static int do_ttn(int argc, char *argv[])
{
    if (argc < 2) {
        print("Available '%s' commands:\n", argv[0]);
        show_help(ttn_commands);
        return CMD_OK;
    }

    const cmd_t *cmd = cmd_find(ttn_commands, argv[1]);
    if (cmd == NULL) {
        print("Unhandled '%s', available commands:\n", argv[1]);
        show_help(ttn_commands);
        return CMD_OK;
    }
    return cmd->cmd(argc - 1, argv + 1);
}

static int do_help(int argc, char *argv[]);
static const cmd_t commands[] = {
    { "led", do_led, "<r g b> or <rgb-hexcode> set RGB led" },
    { "lora", do_lora, "<0|1> set the LoRa LED" },
    { "ttn", do_ttn, "TTN operations" },
    { "help", do_help, "Show help" },
    { NULL, NULL, NULL }
};

static int do_help(int argc, char *argv[])
{
    show_help(commands);
    return CMD_OK;
}

void loop(void)
{
    static int last_period = -1;
    static bool button_pressed = false;

    unsigned long ms = millis();

    if (USB_CABLE_CONNECTED) {
        // handle commands from serial console
        set_lora_led((ms / 500) % 2);

        bool haveLine = false;
        if (Serial.available()) {
            char c;
            haveLine = EditLine(Serial.read(), &c);
            Serial.write(c);
        }
        if (haveLine) {
            int result = cmd_process(commands, line);
            switch (result) {
            case CMD_OK:
                print("OK\n");
                break;
            case CMD_NO_CMD:
                break;
            case CMD_PARAMS:
                print("Invalid params for %s\n", line);
                show_help(commands);
                break;
            case CMD_UNKNOWN:
                print("Unknown command, available commands:\n");
                show_help(commands);
                break;
            default:
                print("%d\n", result);
                break;
            }
            print(">");
        }
    } else {
        // check button, if pressed clear LED and mark button_pressed
        if (!button_pressed && get_button_value()) {
            set_rgb_led(0, 0, 0);
            button_pressed = true;
        }

        // send periodic poll
        int period = millis() / SEND_INTERVAL_MS;
        if (period != last_period) {
            last_period = period;
            uint8_t data = get_rotary_value();
            if (button_pressed) {
                data |= 128;
                button_pressed = false;
            }
            set_lora_led(true);
            ttn.sendBytes(&data, 1, 1, false, 7);
            set_lora_led(false);
        }
    }
}

