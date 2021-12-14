/**
 * Author: Jason White
 *
 * Description:
 * Reads joystick/gamepad events and displays them.
 *
 * Compile:
 * gcc joystick.c -o joystick
 *
 * Run:
 * ./joystick [/dev/input/jsX]
 *
 * See also:
 * https://www.kernel.org/doc/Documentation/input/joystick-api.txt
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <iostream>
#include <string>
#include <map>
#include <queue>
#include <tuple>

// Logitech 
// Axis Events
// 0 - Left Stick X (-32767 (left): 32767 (right)) Default 0
// 1 - Left Stick Y (-32767 (top): 32767 (bottom)) Default 0  
// 2 - Left Trigger (-32767 (not pressed) : 32767 (fully pressed)) Default -32767
// 3 - Right Stick X (-32767 (left): 32767 (right)) Default 0
// 4 - Right Stick Y (-32767 (top): 32767 (bottom)) Default 0  
// 5 - Right Trigger (-32767 (not pressed) : 32767 (fully pressed)) Default -32767
// 6 - Cross bar X (-32767 (left) ->0<- 32767 (right)): 0 these are only 3 values on the axis 
// 7 - Cross bar Y (-32767 (top) ->0<- 32767 (bottom)): 0 these are only 3 values on the axis 

// Buttons (default released)
// 0 - A (green) 
// 1 - B (red) 
// 2 - X (blue) 
// 3 - Y (yellow) 
// 4 - Left trigger 
// 5 - Right trigger
// 6 - Back 
// 7 - Start 
// 8 - Mode (this does not seem to work)
// 9 - Left stick press 
// 10 - Right stick press
// 

//TODO #3
// Testing change from Target 

// Adding another change from Mac remote

// Another change from Mac code 
class GameControllerBase
{
protected:
    std::string _device;
    std::string _name;
    int _fd = -1;
    char _axisCount = 0;
    char _buttonCount = 0;
    js_event _eventBuffer[100];

public:
    // device should be in the format "/dev/input/jsx"
    GameControllerBase(std::string device)
    {
        _device = device;
        _fd = open(_device.c_str(), O_RDONLY);

        if (_fd == -1)
        {
            // having all sorts of problems with std::string and concat +
            char err[256];
            sprintf(err, "Could not open game controller: %s\n", _device.c_str());
            perror(err);
            return;
        }
        char name[128];
	    if (ioctl(_fd, JSIOCGNAME(sizeof(name)), name) < 0)
            strncpy(name, "Unknown", sizeof(name));
        _name = name;

	    ioctl (_fd, JSIOCGAXES, &_axisCount);
	    ioctl (_fd, JSIOCGBUTTONS, &_buttonCount);
    }

    void cleanup()
    {
        if(_fd != -1)
            close(_fd);
        _fd = -1;
    }

    ~GameControllerBase()
    {
        cleanup();
    }

    // Getters
    uint getAxisCount() const { return _axisCount;};
    uint getButtonCount() const { return _buttonCount;};
    const char * getDevice() const { return _device.c_str();};
    const char * getName() const { return _name.c_str();};

    typedef std::queue<js_event> EventQueue;

    // Can be used where there is no derived class
    int waitForEvent(EventQueue& q)
    {
	    int bytes = read (_fd, _eventBuffer, sizeof(_eventBuffer));
        int events = bytes/sizeof(js_event);
        for(int i=0; i < events; i++)
        {
            q.push(_eventBuffer[i]);
        }
        return events;
    }

protected:
    std::tuple<int, js_event *> readEvents()
    {
       	int bytes = read (_fd, _eventBuffer, sizeof(_eventBuffer));
        int events = bytes/sizeof(js_event); 
        return {events, _eventBuffer};
    }

};

// TODO determine the type based on the dev/input/jsx -> name 
// Create a factory or static function to instantiate the type of controller
class LogitechF710: public GameControllerBase
{
public:
    enum BUTTON
    {
        A,      // Green
        B,      // Red
        X,      // Blue
        Y,      // Yellow
        LT,     // Left trigger
        RT,     // Right trigger
        BACK,   // Back button
        START,  // Start Button
        MODE,   // Mode button
        LS,     // Left stick button
        RS,     // Right stick button
        BUTTON_COUNT   // Number of buttons
    };

    enum AXIS
    {
        LEFT_STICK_X,   //(-32767 (left): 32767 (right)) Default 0 
        LEFT_STICK_Y,   //(-32767 (left): 32767 (right)) Default 0    
        LEFT_TRIGGER,   //(-32767 (not pressed) : 32767 (fully pressed)) Default -32767
        RIGHT_STICK_X,  //(-32767 (left): 32767 (right)) Default 0 
        RIGHT_STICK_Y,  //(-32767 (left): 32767 (right)) Default 0    
        RIGHT_TRIGGER,  //(-32767 (not pressed) : 32767 (fully pressed)) Default -32767
        CROSSBAR_X,     //(-32767 (left) ->0<- 32767 (right)): 0 these are only 3 values on the axis  
        CROSSBAR_Y,     //(-32767 (left) ->0<- 32767 (right)): 0 these are only 3 values on the axis 
        AXIS_COUNT      // Number of axis 
    };
 
    struct ButtonState
    {
        enum STATE {PRESSED, RELEASED};
        STATE state;

        bool enabled; // reporting on this button enable
    };

    struct AxisConfig
    {
        std::string name;
        typedef struct
        {
            int low;
            int high;
            int mid;
            int latent; // Value when not pressed
        } Range;

        Range range;
    };

protected:
  static std::string signature;
  static std::map<BUTTON, std::string> buttonConfig;
  static std::map<AXIS, AxisConfig> axisConfig;
public:
    LogitechF710(std::string device): GameControllerBase(device)
    {
        if (LogitechF710::signature != getName())
        {
            cleanup();

            char err[256];
            sprintf(err, "Device %s:%s is not a %s controller", device.c_str(), getName(), LogitechF710::signature.c_str());
            throw std::invalid_argument( err);
        }
    }

    // Functor object - Used for threading
    void operator()(int a)
    {
        eventLoop();
    }

    void eventLoop()
    {
       while(1)
       {
           int events=0;
           js_event * buffer;
           // auto [events, buffer] not available in c++11
           std::tie(events, buffer) = readEvents();

            // controller unplugged
            if( events < 0){
                return ;
            }

            for (int i=0; i < events; i++, buffer++)
            {
                switch (buffer->type)
                {
                case JS_EVENT_BUTTON:

                    if (buffer->number < 0 || buffer->number >= BUTTON::BUTTON_COUNT)
                    {
                        printf("Button %u %s undefined\n", buffer->number, buffer->value ? "pressed" : "released");
                    }
                    else
                    {
                        auto b = static_cast<BUTTON> (buffer->number);
                        if (buttonConfig.count(b))
                        {
                            std::string name = buttonConfig[b];
                            printf("%s %s\n", name.c_str(), buffer->value ? "pressed" : "released");
                        }
                    }
                    break;

                case JS_EVENT_AXIS:
                    if (buffer->number < 0 || buffer->number >= AXIS::AXIS_COUNT)
                    {
                        printf("Axis %u \n", buffer->number);
                    }
                    else
                    {
                        auto b = static_cast<AXIS> (buffer->number);
                        if (axisConfig.count(b))
                        {
                            AxisConfig config = axisConfig[b];
                            printf("%s value %d\n", config.name.c_str(), buffer->value);
                        }
                    }
                    break;

                default:
                    /* Ignore init events. */
                    printf("Default %u\n", buffer->type);
                    break;
                }
            }
        } 
    }
};

std::string LogitechF710::signature = "Logitech Gamepad F710";
std::map<LogitechF710::BUTTON, std::string> LogitechF710::buttonConfig
{
    { LogitechF710::BUTTON::A, "A - Green"},
    { LogitechF710::BUTTON::B, "B - Red"},
    { LogitechF710::BUTTON::X, "X - Blue"},
    { LogitechF710::BUTTON::Y, "Y - Yellow"},
    { LogitechF710::BUTTON::LT, "Left Trigger"},
    { LogitechF710::BUTTON::RT, "Right Trigger"},
    { LogitechF710::BUTTON::BACK, "Back"},
    { LogitechF710::BUTTON::START, "Start"},
    { LogitechF710::BUTTON::MODE, "Mode"},
    { LogitechF710::BUTTON::LS, "Left Stick"},
    { LogitechF710::BUTTON::RS, "Right Stick"}
};

std::map<LogitechF710::AXIS, LogitechF710::AxisConfig> LogitechF710::axisConfig
{
    { LogitechF710::AXIS::LEFT_STICK_X, {"Left Stick-X", {-32767, 32767, 0, 0}}},
    { LogitechF710::AXIS::LEFT_STICK_Y, {"Left Stick-Y", {-32767, 32767, 0, 0}}}, 
    { LogitechF710::AXIS::LEFT_TRIGGER, {"Left Trigger", {-32767, 32767, 0, -32767}}},  
    { LogitechF710::AXIS::RIGHT_STICK_X, {"Right Stick-X", {-32767, 32767, 0, 0}}},
    { LogitechF710::AXIS::RIGHT_STICK_Y, {"Right Stick-Y", {-32767, 32767, 0, 0}}}, 
    { LogitechF710::AXIS::RIGHT_TRIGGER, {"Right Trigger", {-32767, 32767, 0, -32767}}}, 
    { LogitechF710::AXIS::CROSSBAR_X, {"Crossbar-X", {-32767, 32767, 0, 0}}}, 
    { LogitechF710::AXIS::CROSSBAR_Y, {"Crossbar-Y", {-32767, 32767, 0, 0}}},        
};

