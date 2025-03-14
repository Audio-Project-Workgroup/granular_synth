/* File to define new tests for the midi-parsing implementation*/

void addNoteOnMessage(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0x90;//note on
    atMidiBuffer[6] = 0x48;
    atMidiBuffer[7] = 0x5f;
    atMidiBuffer+=7*sizeof(uint8_t);
}

void addNoteOffMessage(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0x80;//note off
    atMidiBuffer[6] = 0x48;
    atMidiBuffer[7] = 0x5f;
    atMidiBuffer+=7*sizeof(uint8_t);

}
void addCCMessage_1(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xB0; //CC message
    atMidiBuffer[6] = 0x01; // Controller
    atMidiBuffer[7] = 0x02; // Value
    atMidiBuffer+=7*sizeof(uint8_t);
}

// void add_new_message(uint8_t *atMidiBuffer){
    // adding a new message, you define a new test. This is done with:
    
    // 1. fill sequential bytes with the message data
    // atMidiBuffer[0] = ...;
    // ...
    // atMidiBuffer[N] = ...;
    
    // 2. increment the atMidiBuffer pointer by the number of bytes you filled in:
    // atMidiBuffer+=N*sizeof(uint8_t);

// }