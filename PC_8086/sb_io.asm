;///////////////////////////////////////////////////////////////////////////////////
;//-------------------------------------------------------------------------------//
;//-------------------------------------------------------------------------------//
;//-----------H----H--X----X-----CCCCC----22222----0000-----0000------11----------//
;//----------H----H----X-X-----C--------------2---0----0---0----0--1--1-----------//
;//---------HHHHHH-----X------C----------22222---0----0---0----0-----1------------//
;//--------H----H----X--X----C----------2-------0----0---0----0-----1-------------//
;//-------H----H---X-----X---CCCCC-----222222----0000-----0000----1111------------//
;//-------------------------------------------------------------------------------//
;//----------------------------------------------------- http://hxc2001.free.fr --//
;///////////////////////////////////////////////////////////////////////////////////
;// File : sb_io.asm
;// Contains: Low level Sound blaster IO function and IRQ vector
;//
;// Written by: Jean François DEL NERO
;///////////////////////////////////////////////////////////////////////////////////

.MODEL LARGE

.STACK 512

.DATA

PUBLIC     _it_flag
PUBLIC     _it_toggle
PUBLIC     _it_sbport
PUBLIC     _it_irq

_it_flag   db 0
_it_toggle db 0
_it_irq    db 0
_it_sbport dw 0220h

it_old_handler_seg dw 0
it_old_handler_off dw 0

;-----------------------------------------------
.CODE

sb_irq_ proc
	push dx
	push ax
	push ds

	mov ax, @DATA
	mov ds, ax

	mov dx, ds:[_it_sbport]
	add dx,0eh
	in  al,dx

	mov al,0FFh
	mov ds:[_it_flag],al

	mov al,ds:[_it_toggle]
	not al
	mov ds:[_it_toggle],al

	mov dx,20h
	mov al,20h
	out dx,al

	pop ds
	pop ax
	pop dx

	iret
sb_irq_ endp


;-----------------------------------------------

install_irq_ proc public
	push ax
	push dx
	push es
	push bx
	push ds

	cli

	mov ax, @DATA
	mov ds, ax

	; save the existing interrupt handler
	mov ah, 35h
	mov al, ds:[_it_irq]
	add al, 8h
	int 21h
	mov ds:[it_old_handler_seg], es
	mov ds:[it_old_handler_off], bx

	; set our it handler
	mov ah, 25h
	mov al, ds:[_it_irq]
	add al, 8h
	mov dx, sb_irq_
	mov bx, cs
	mov ds, bx
	int 21h

	sti

	pop ds
	pop bx
	pop es
	pop dx
	pop ax
	ret
install_irq_ endp

;-----------------------------------------------

uninstall_irq_ proc public
	push ax
	push dx
	push ds

	cli

	mov ax, @DATA
	mov ds, ax

	; restore the old it handler
	mov ah, 25h
	mov al, ds:[_it_irq]
	add al, 8h
	mov dx, ds:[it_old_handler_off]
	mov ds, ds:[it_old_handler_seg]
	int 21h

	sti

	pop ds
	pop dx
	pop ax
	ret
uninstall_irq_ endp

;-----------------------------------------------

SB_DSP_wr_ proc public
	push dx
	push ax

	mov dx,ax

	add dx, 0Ch

wait_wr_dsp_loop:

	in al, dx
	test al, 080h
	jnz wait_wr_dsp_loop

	pop ax

	pop ax
	push ax

	out dx, al

	pop dx

	ret
SB_DSP_wr_ endp

;-----------------------------------------------

SB_DSP_rd_ proc public
	push dx

	mov dx,ax

	add dx, 0Eh

wait_rd_dsp_loop:
	in al, dx
	test al, 080h
	jz wait_rd_dsp_loop

	sub dx,4

	in al,dx

	pop dx

	ret
SB_DSP_rd_ endp

END
