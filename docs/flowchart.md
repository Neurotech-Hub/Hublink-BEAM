```mermaid
%%{init: {
    'theme': 'dark',
    'themeVariables': {
        'fontFamily': 'arial',
        'fontSize': '16px',
        'primaryColor': '#1a1a1a',
        'primaryTextColor': '#fff',
        'primaryBorderColor': '#fff',
        'lineColor': '#666',
        'secondaryColor': '#2a2a2a',
        'tertiaryColor': '#2a2a2a'
    }
}}%%
flowchart TB
    title[Hublink BEAM Flowchart]
    style title fill:none,stroke:none,color:#fff,font-size:24px,font-weight:bold
    
    %% ULP Program Flow - Starts here after deep sleep
    subgraph ULP [ULP Program Loop]
        direction TB
        ULPStart[Start ULP Program] --> ReadGPIO[Read GPIO3/SDA]
        ReadGPIO --> Motion{Motion\nDetected?}
        Motion -- Yes --> IncPIR[Increment PIR Count]
        IncPIR --> ResetTracker[Reset Inactivity\nTracker]
        Motion -- No --> IncTracker[Increment Inactivity\nTracker]
        IncTracker --> CheckPeriod{Tracker >=\nPeriod?}
        CheckPeriod -- Yes --> IncInactive[Increment Inactivity\nCount]
        IncInactive --> ResetTracker
        CheckPeriod -- No --> Delay[1-Second Delay]
        ResetTracker --> Delay
        Delay --> ReadGPIO
    end
    
    %% Main Program Flow - After ULP or power on
    Start[Power On/Reset] --> Init[Initialize HublinkBEAM]
    ULP --> Init
    Init --> CheckWake{Wake from\nDeep Sleep?}
    
    %% First Boot Path
    CheckWake -- No --> FirstBoot[First Boot Setup]
    FirstBoot --> InitSensors[Initialize Sensors]
    InitSensors --> InitSD[Initialize SD Card]
    InitSD --> ClearCounters[Clear PIR & Inactivity\nCounters]
    
    %% Wake from Sleep Path
    CheckWake -- Yes --> CalcMetrics[Calculate Activity Metrics]
    CalcMetrics --> ReadPIR[Read PIR Count]
    ReadPIR --> CalcActive[Calculate Active Time]
    CalcActive --> CalcInactive[Calculate Inactivity\nFraction]
    
    %% Main Loop from BasicLoggingHublink.ino
    CalcInactive --> LogData[Log Data to SD]
    ClearCounters --> LogData
    LogData --> CheckAlarm{Check Alarm\nInterval}
    CheckAlarm -- Yes --> SyncData[Sync with Hublink]
    CheckAlarm -- No --> PrepSleep[Prepare for Sleep]
    SyncData --> PrepSleep
    
    %% Sleep Preparation
    PrepSleep --> ConfigULP[Configure ULP Program]
    ConfigULP --> StartULP[Start ULP & Enter\nDeep Sleep]
    StartULP --> ULP

    %% Styling
    classDef process fill:#2d3436,stroke:#00cec9,stroke-width:2px,color:#fff
    classDef decision fill:#2d3436,stroke:#ffeaa7,stroke-width:2px,color:#fff
    classDef ulp fill:#2d3436,stroke:#fd79a8,stroke-width:2px,color:#fff
    
    class Start,Init,FirstBoot,InitSensors,InitSD,ClearCounters,LogData,PrepSleep,ConfigULP,StartULP process
    class CheckWake,CheckAlarm,Motion,CheckPeriod decision
    class ULPStart,ReadGPIO,IncPIR,ResetTracker,IncTracker,IncInactive,Delay ulp
``` 