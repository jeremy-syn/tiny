Opening the project in STM32Cube IDE -- Do I open .../benchmark/interface or .../benchmark/interface/STM32CubeIDE?  If I open interface, it tries to open two projects (interface and CubeIDE).
 - Neither of those worked.  I could click through without any errors, but then it still said there were no projects in the workspace, even after "open project from filesystem"

Trying "start project from existing cubemx config file (*.ioc file)"
 - starts a long download
 - Now I can at least see code in the project explorer view
 - This seems better, but I get some build errors w/ missing files.  It seems like it might be confused about whether stuff is in
   interface or interface/STM32CubeIDE.


Trying "File->Import.. you choose General->Import existing Projects into workspace"
  - Select .../benchmark (not interface) as workspace directory.
  - Now the project shows up, already imported.  So I think something I did previously imported it. And now it builds with no errors.
  

Import Projects ... ; General, existing projects into workspace.
You can select root of whole repo.  it will find all prjoects

Select interface/STM32CubeIDE


* USART3 => PB10,11 = M10,N12 = ARD_D1,ARD_D0 = CN15.2,1, the 1x wide connector on the 
  bottom, on the right with the board upside down and headphone jack at top.  This is the 
  one the DUT is meant to connect to.
* So USART1 should be the UART that the host connects to. huart1 is UART_HandleTypeDef, and  huart1.Instance = USART1.
  - On mac, screen does not really support 921600 baud; it will revert back to 9600.  So to connect at 921600 baud, you can start screen (`screen -L /dev/cu.usbmodem103 921600`, though I don't think the 921600 is needed) and then _in another terminal, once screen has started_, set the baudrate to 921600 with `sudo stty -f /dev/cu.usbmodem103 921600`.  The set to 921600 only lasts for that one screen session.

## Questions for Steve
### Interface S/W
* BaudRate = 921600 ? Yes.  
* Menu::HandleCommand() is a little hard to follow
* 

* Is there any existing documentation for this?
* What is implemented and what is left?
* What blocks of code do what?
* For the things to be implemented, where are the hooks to connect to, where should they be implemented?


Runner has

* Blue LED was not on; nothing blinking
* Green LED will toggle when a task is received.

Task Poller in (./FileX/App/app_filex.c
./FileX/App/app_filex.c


Currently LED is off, then wait for SD, then turn Blue LED back on. Try reversing it. 


If there is a wav file on on SD card, set headphone play flag, you should be able to listen to it.


CLI_Init() is in InterfaceMenu.c, and it's called in App_ThreadX_Init() in app_threadx.c, which in turn is called from tx_application_define in app_azure_rtos.c

The for(;;) loop in the block labeled fx_app_thread_entry in app_filex.c seems to be the main control loop.

Menu::Run() in Menu.cpp monitors the UART input from the host.

** Current question: Why does a breakpoint in Menu:Run() never stop execution? (I'm typing 'name%' in screen.)  Menu::Run() I think is called in InterfaceMenu.cpp in CLI_Run(), which is called in tx_app_thread_entry() in app_threadx.c, which is launched in a call to tx_thread_create() in App_ThreadX_Init(), still in app_threadx.c.


 
main()=>
app_threadx.c:MX_ThreadX_Init()=>
tx_initialize_kernel_enter.c:_tx_initialize_kernel_enter()=>
app_azure_rtos.c:tx_application_define()=>
app_threadx.c:App_ThreadX_Init()