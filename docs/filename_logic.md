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
flowchart TD
    %% Title handling
    title[Filename Selection Logic]
    style title fill:none,stroke:none,color:#fff,font-size:20px,font-weight:bold

    %% Main flow
    Start([Start getCurrentFilename])
    A{Stored filename exists?}
    B{Wake from sleep?}
    C{File from today?}
    D{newFileOnBoot?}
    E{newFileOnBoot?}
    F{Any files from today?}
    UseStored[Use stored filename]
    UseFirst[Use first existing file]
    ScanNew[Scan for next available number]
    End([Return filename])

    %% Connections
    Start --> A
    A -->|No| D
    A -->|Yes| B
    B -->|Yes| C
    B -->|No| E
    C -->|Yes| UseStored
    C -->|No| D
    E -->|Yes| D
    E -->|No| C
    D -->|Yes| ScanNew
    D -->|No| F
    F -->|Yes| UseFirst
    F -->|No| ScanNew
    UseStored --> End
    UseFirst --> End
    ScanNew --> End

    %% Styling
    classDef default fill:#2d3436,stroke:#00cec9,stroke-width:2px,color:#fff
    classDef decision fill:#2d3436,stroke:#ffeaa7,stroke-width:2px,color:#fff
    classDef process fill:#2d3436,stroke:#00cec9,stroke-width:2px,color:#fff
    classDef start fill:#2d3436,stroke:#fd79a8,stroke-width:3px,color:#fff
    classDef endNode fill:#2d3436,stroke:#fd79a8,stroke-width:3px,color:#fff

    %% Apply classes
    class A,B,C,D,E,F decision
    class UseStored,UseFirst,ScanNew process
    class Start start
    class End endNode
``` 