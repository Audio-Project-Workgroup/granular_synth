/* File to define new tests for the midi-parsing implementation*/

size_t addNoteOnMessage(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0x90;//note on
    atMidiBuffer[6] = 0x48;
    atMidiBuffer[7] = 0x5f;
    return 8*sizeof(uint8_t);

}

size_t addNoteOffMessage(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0x80;//note off
    atMidiBuffer[6] = 0x48;
    atMidiBuffer[7] = 0x5f;
    return 8*sizeof(uint8_t);

}
size_t addCCMessage_1(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xB0; //CC message
    atMidiBuffer[6] = 0x01; // Controller
    atMidiBuffer[7] = 0x02; // Value
    return 8*sizeof(uint8_t);
}

size_t addCCMessage_2(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xB0; //CC message
    atMidiBuffer[6] = 0x01; // Controller
    atMidiBuffer[7] = 0xFF; // Value
    return 8*sizeof(uint8_t);
}

size_t addCCMessage_3(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xB0; //CC message
    atMidiBuffer[6] = 0x01; // Controller
    atMidiBuffer[7] = 0xFE; // Value
    return 8*sizeof(uint8_t);
}

size_t addCCMessage_4(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xB0; //CC message
    atMidiBuffer[6] = 0x01; // Controller
    atMidiBuffer[7] = 0x7F; // Value
    return 8*sizeof(uint8_t);
}

size_t addCCMessage_5(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xB0; //CC message
    atMidiBuffer[6] = 0x05; // Controller
    atMidiBuffer[7] = 0x80; // Value
    return 8*sizeof(uint8_t);
}

size_t addCCMessage_6(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xB0; //CC message
    atMidiBuffer[6] = 0x03; // Controller
    atMidiBuffer[7] = 0x81; // Value
    return 8*sizeof(uint8_t);
}

size_t addPitchBend_1(uint8_t *atMidiBuffer){
    atMidiBuffer[0] = 0x07;
    atMidiBuffer[1] = 0x55;
    atMidiBuffer[2] = 0x00;
    atMidiBuffer[3] = 0x00;
    atMidiBuffer[4] = 0x00;
    atMidiBuffer[5] = 0xE0; //Pitch Bend
    atMidiBuffer[6] = 0x00; // lsb = 0
    atMidiBuffer[7] = 0x40; // msb = 64 --> 0x40
                            // this should result in 8192, which means no bend.
    return 8*sizeof(uint8_t);
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