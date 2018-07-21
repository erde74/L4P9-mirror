#include INC_ARCH(ioport.h)		/* for in_u8/out_u8	*/

/**
 * Driver for i8259 PIC
 * @param base	the base address of the registers
 *
 * The template parameter BASE enables compile-time resolution of the
 * PIC's control register addresses.
 *
 * Note:
 *   Depending on whether I8259_CACHE_PICSTATE is defined or not
 *   objects will cache the mask register or not. Thus it is not wise
 *   to blindly instanciate them all over the place because the cached
 *   state would not be shared. Making the cached state static
 *   wouldn't work either because there are two PICs in a
 *   PC99. Intended use is a single object per PIC.
 *
 * Assumptions:
 * - BASE can be passed as port to in_u8/outb
 * - The PIC's A0=0 register is located at BASE
 * - The PIC's A0=1 register is located at BASE+1
 * - PICs in unbuffered cascade mode
 *
 * Uses:
 * - outb, inb
 */


void unmask(word_t irq)
{
	    u8_t mask_cache = inb(base+1);
	    mask_cache &= ~(1 << (irq)); 
	    outb(base+1, mask_cache);   
}

/**	Mask interrupt
 *	@param irq	interrupt line to mask
 */
void mask(word_t irq)
{

	    u8_t mask_cache = inb(base+1);
	    mask_cache |= (1 << (irq)); 
	    outb(base+1, mask_cache);   
}

/**	Send specific EOI
 *	@param irq	interrupt line to ack
 */
void ack(word_t irq)
{
	    outb(base, 0x60 + irq);   
}

/**	Check if interrupt is masked	
 *  @param irq      interrupt line
 *  @return         true when masked, false otherwise
 */	
bool is_masked(word_t irq)	
{	
	    u8_t mask_cache = inb(base+1);
	    return (mask_cache & (1 << irq));	
}

/**
 *	initialize PIC
 *	@param vector_base	8086-style vector number base
 *	@param slave_info	slave mask for master or slave id for slave
 *
 *	Initializes the PIC in 8086-mode:
 *  - not special-fully-nested mode
 *	- reporting vectors VECTOR_BASE...VECTOR_BASE+7
 *	- all inputs masked
 */
oid init(u8_t vector_base, u8_t slave_info)
{
	    u8_t    mask_cache = 0xFF;

	    /*  ICW1: 0x10 | NEED_ICW4 | CASCADE_MODE | EDGE_TRIGGERED */
	    outb(base, 0x11);

	    /* ICW2:  - 8086 mode irq vector base
	     *    PIN0->IRQ(base), ..., PIN7->IRQ(base+7)
	     */

	    outb(base+1, vector_base);

	    /* ICW3:
	       - master: slave list
	         Set bits mark input PIN as connected to a slave
	       - slave: slave id
	         This PIC is connected to the master's pin SLAVE_ID
	       Note: The caller knows whether its a master or not -
	             the handling is the same.
	    */
	    outb(base+1, slave_info);

	    /*  ICW4:
	        8086_MODE | NORMAL_EOI | NONBUFFERED_MODE | NOT_SFN_MODE
	     */
	    outb(base+1, 0x01); /* mode - *NOT* fully nested */

	    /*  OCW1:  - set initial mask   */
	    outb(base+1, mask_cache);
}
