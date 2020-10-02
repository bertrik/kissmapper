#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <TheThingsNetwork.h>

#include "editline.h"
#include "cmdproc.h"

#define PIN_LED_RED     12      // PD6
#define PIN_LED_GREEN   6       // PD7
#define PIN_LED_BLUE    11      // PB7
#define PIN_LORA_LED    8       // PB4 xx
#define PIN_ROTARY_SW0  5       // PC6
#define PIN_ROTARY_SW1  13      // PC7
#define PIN_ROTARY_SW2  9       // PB5
#define PIN_ROTARY_SW3  30      // PD5
#define PIN_BUTTON      7       // PE6

//Define both serial interfaces for easier use
#define loraSerial			Serial1
#define usbserial			Serial  //for commisioning and debugging

//Function prototypes
static void set_lora_led(bool state);
static void set_rgb_led(uint8_t R, uint8_t G, uint8_t B);

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

    //set the analog reference to the internal reference
    analogReference(INTERNAL);

    //Start serial ports
    loraSerial.begin(57600);    //RN2483 needs 57600 baud rate  
    usbserial.begin(115200);

    EditInit(line, sizeof(line));

    // yellow while initializing
    set_rgb_led(1, 1, 0);

    usbserial.println(F("--- Resetting RN module"));
    ttn.reset(false);

    // attempt to resume from previously set session
    ttn_ok = ttn.personalize();

    // green when initialized, red when error
    if (ttn_ok) {
        set_rgb_led(0, 1, 0);
    } else {
        set_rgb_led(1, 0, 0);
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

//! \brief Turn RGB led on with a given state colour of turn it off
static void set_rgb_led(uint8_t r, uint8_t g, uint8_t b)
{
    digitalWrite(PIN_LED_RED, !r);
    digitalWrite(PIN_LED_GREEN, !g);
    digitalWrite(PIN_LED_BLUE, !b);
}

static void show_help(const cmd_t * cmds)
{
    for (const cmd_t * cmd = cmds; cmd->cmd != NULL; cmd++) {
        print("%10s: %s\n", cmd->name, cmd->help);
    }
}

static int do_led(int argc, char *argv[])
{
    if (argc < 4) {
        return CMD_PARAMS;
    }
    int r = (strcmp("0", argv[1]) != 0);
    int g = (strcmp("0", argv[2]) != 0);
    int b = (strcmp("0", argv[3]) != 0);
    print("set_rgb_led(%d,%d,%d)\n", r, g, b);
    set_rgb_led(r, g, b);
    return CMD_OK;
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

static int do_ttn_poll(int argc, char *argv[])
{
    return (ttn.poll() == TTN_SUCCESSFUL_TRANSMISSION) ? CMD_OK : -1;
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

// ttn subcommands
static const cmd_t ttn_commands[] = {
    { "reset", do_ttn_reset, "Resets the RN2483" },
    { "status", do_ttn_status, "Shows RN2483 status" },
    { "join", do_ttn_join, "Try to join" },
    { "poll", do_ttn_poll, "Poll the network" },
    { "abp", do_ttn_abp, "<devadr> <nwkkey> <seskey> Perform ABP" },
    { "otaa", do_ttn_otaa, "<appeui> <appkey> Perform OTAA" },
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
    { "led", do_led, "<r|g|b> <0|1> set red, green or blue led" },
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
    static unsigned long last_sent = 0;

    // parse command line
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

    // clear error condition with button press
    if (!ttn_ok) {
        if (get_button_value()) {
            print("Clearing error condition\n");
            ttn_ok = true;
        }
    }

    // send periodic poll
    unsigned long ms = millis();
    if ((ms - last_sent) > 10000) {
        last_sent = ms;
        if (ttn_ok) {
            set_rgb_led(0, 0, 1);
            ttn_ok = (ttn.poll() == TTN_SUCCESSFUL_TRANSMISSION);
            if (ttn_ok) {
                set_rgb_led(0, 0, 0);
            } else {
                set_rgb_led(1, 0, 0);
            }
        }
    }

}
