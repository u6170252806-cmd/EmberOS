; CASM File I/O Demo - saves name+age to profile.txt
; Demonstrates: inps, fwrite, strb, ldrb
; Data area starts at 0x400 (after code)

.text
_start:
    ; Print "Name? "
    mov w0, #78
    prtc
    mov w0, #97
    prtc
    mov w0, #109
    prtc
    mov w0, #101
    prtc
    mov w0, #63
    prtc
    mov w0, #32
    prtc

    ; Read name into 0x400 (data area)
    mov x0, #0x400
    mov x1, #24
    inps
    mov x20, x0         ; save length

    ; Print "Age? "
    mov w0, #65
    prtc
    mov w0, #103
    prtc
    mov w0, #101
    prtc
    mov w0, #63
    prtc
    mov w0, #32
    prtc

    ; Read age into 0x430
    mov x0, #0x430
    mov x1, #8
    inps
    mov x21, x0

    ; Build output at 0x500: "Name: xxx\nAge: xxx\n"
    mov x10, #0x500

    ; "Name: "
    mov w0, #78
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #97
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #109
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #101
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #58
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #32
    strb w0, [x10]
    add x10, x10, #1

    ; Copy name
    mov x11, #0x400
    mov x12, x20
cp_name:
    cmp x12, #0
    b.eq cp_name_done
    ldrb w0, [x11]
    strb w0, [x10]
    add x10, x10, #1
    add x11, x11, #1
    sub x12, x12, #1
    b cp_name
cp_name_done:

    ; "\nAge: "
    mov w0, #10
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #65
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #103
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #101
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #58
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #32
    strb w0, [x10]
    add x10, x10, #1

    ; Copy age
    mov x11, #0x430
    mov x12, x21
cp_age:
    cmp x12, #0
    b.eq cp_age_done
    ldrb w0, [x11]
    strb w0, [x10]
    add x10, x10, #1
    add x11, x11, #1
    sub x12, x12, #1
    b cp_age
cp_age_done:

    ; "\n\0"
    mov w0, #10
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #0
    strb w0, [x10]

    ; Length
    sub x22, x10, #0x500

    ; Filename at 0x480: "profile.txt"
    mov x10, #0x480
    mov w0, #112
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #114
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #111
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #102
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #105
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #108
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #101
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #46
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #116
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #120
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #116
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #0
    strb w0, [x10]

    ; Write file
    mov x0, #0x480
    mov x1, #0x500
    mov x2, x22
    fwrite

    ; Print "Saved!"
    mov w0, #83
    prtc
    mov w0, #97
    prtc
    mov w0, #118
    prtc
    mov w0, #101
    prtc
    mov w0, #100
    prtc
    mov w0, #33
    prtc
    mov w0, #10
    prtc

    halt
