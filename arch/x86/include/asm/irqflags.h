#ifndef _X86_IRQFLAGS_H_
#define _X86_IRQFLAGS_H_

#include <asm/processor-flags.h>

#ifndef __ASSEMBLY__

#include <linux/ipipe_base.h>
#include <linux/ipipe_trace.h>
#include <linux/compiler.h>

/*
 * Interrupt control:
 */

static inline unsigned long native_save_fl(void)
{
	unsigned long flags;

	/*
	 * "=rm" is safe here, because "pop" adjusts the stack before
	 * it evaluates its effective address -- this is part of the
	 * documented behavior of the "pop" instruction.
	 */
	asm volatile("# __raw_save_flags\n\t"
		     "pushf ; pop %0"
		     : "=rm" (flags)
		     : /* no input */
		     : "memory");

	return flags;
}

static inline void native_restore_fl(unsigned long flags)
{
	asm volatile("push %0 ; popf"
		     : /* no output */
		     :"g" (flags)
		     :"memory", "cc");
}

static inline void native_irq_disable(void)
{
	asm volatile("cli": : :"memory");
}

static inline void native_irq_enable(void)
{
	asm volatile("sti": : :"memory");
}

static inline void native_safe_halt(void)
{
	asm volatile("sti; hlt": : :"memory");
}

static inline void native_halt(void)
{
	asm volatile("hlt": : :"memory");
}

static inline int native_irqs_disabled(void)
{
	unsigned long flags = native_save_fl();

	return !(flags & X86_EFLAGS_IF);
}

#endif

#ifdef CONFIG_PARAVIRT
#include <asm/paravirt.h>
#else
#ifndef __ASSEMBLY__

static inline unsigned long arch_local_save_flags(void)
{
#ifdef CONFIG_IPIPE
 	unsigned long flags;

	flags = (!__ipipe_test_root()) << 9;
	barrier();
	return flags;
#else
	return native_save_fl();
#endif
}

static inline void arch_local_irq_restore(unsigned long flags)
{
#ifdef CONFIG_IPIPE
	barrier();
	__ipipe_restore_root(!(flags & X86_EFLAGS_IF));
#else
	native_restore_fl(flags);
#endif
}

static inline void arch_local_irq_disable(void)
{
#ifdef CONFIG_IPIPE
	ipipe_check_context(ipipe_root_domain);
	__ipipe_stall_root();
	barrier();
#else
	native_irq_disable();
#endif
}

static inline void arch_local_irq_enable(void)
{
#ifdef CONFIG_IPIPE
	barrier();
	__ipipe_unstall_root();
#else
	native_irq_enable();
#endif
}

/*
 * Used in the idle loop; sti takes one instruction cycle
 * to complete:
 */
static inline void arch_safe_halt(void)
{
#ifdef CONFIG_IPIPE
	barrier();
	__ipipe_halt_root();
#else
	native_safe_halt();
#endif
}

/*
 * Used when interrupts are already enabled or to
 * shutdown the processor:
 */
static inline void halt(void)
{
	native_halt();
}

/* Merge virtual+real interrupt mask bits into a single word. */
static inline unsigned long arch_mangle_irq_bits(int virt, unsigned long real)
{
	return (real & ~(1L << 31)) | ((unsigned long)(virt != 0) << 31);
}

/* Converse operation of arch_mangle_irq_bits() */
static inline int arch_demangle_irq_bits(unsigned long *x)
{
	int virt = (*x & (1L << 31)) != 0;
	*x &= ~(1L << 31);
	return virt;
}

/*
 * For spinlocks, etc:
 */
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags = arch_local_save_flags();
	arch_local_irq_disable();
	return flags;
}
#else

#define ENABLE_INTERRUPTS(x)	sti
#define DISABLE_INTERRUPTS(x)	cli

#ifdef CONFIG_IPIPE
#define ENABLE_INTERRUPTS_HW_COND	sti
#define DISABLE_INTERRUPTS_HW_COND	cli
#else /* !CONFIG_IPIPE */
#define ENABLE_INTERRUPTS_HW_COND
#define DISABLE_INTERRUPTS_HW_COND
#endif /* !CONFIG_IPIPE */

#ifdef CONFIG_X86_64
#define SWAPGS	swapgs
/*
 * Currently paravirt can't handle swapgs nicely when we
 * don't have a stack we can rely on (such as a user space
 * stack).  So we either find a way around these or just fault
 * and emulate if a guest tries to call swapgs directly.
 *
 * Either way, this is a good way to document that we don't
 * have a reliable stack. x86_64 only.
 */
#define SWAPGS_UNSAFE_STACK	swapgs

#define PARAVIRT_ADJUST_EXCEPTION_FRAME	/*  */

#define INTERRUPT_RETURN	iretq
#define USERGS_SYSRET64				\
	swapgs;					\
	sysretq;
#define USERGS_SYSRET32				\
	swapgs;					\
	sysretl
#define ENABLE_INTERRUPTS_SYSEXIT32		\
	swapgs;					\
	sti;					\
	sysexit

#else
#define INTERRUPT_RETURN		iret
#define ENABLE_INTERRUPTS_SYSEXIT	sti; sysexit
#define GET_CR0_INTO_EAX		movl %cr0, %eax
#endif


#endif /* __ASSEMBLY__ */
#endif /* CONFIG_PARAVIRT */

#ifndef __ASSEMBLY__
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & X86_EFLAGS_IF);
}

static inline int arch_irqs_disabled(void)
{
	unsigned long flags = arch_local_save_flags();

	return arch_irqs_disabled_flags(flags);
}

/*
 * FIXME: we should really align on native_* at some point, instead of
 * introducing yet another layer (i.e. *_hw()).
 */
#define local_irq_save_hw_notrace(flags)	\
	do {					\
		(flags) = native_save_fl();	\
		native_irq_disable();		\
	} while (0)

static inline void local_irq_restore_hw_notrace(unsigned long flags)
{
	native_restore_fl(flags);
}

static inline void local_irq_disable_hw_notrace(void)
{
	native_irq_disable();
}

static inline void local_irq_enable_hw_notrace(void)
{
	native_irq_enable();
}

static inline int irqs_disabled_hw(void)
{
	return native_irqs_disabled();
}

#ifdef CONFIG_IPIPE_TRACE_IRQSOFF

#define local_irq_disable_hw() do {			\
		if (!native_irqs_disabled()) {		\
			native_irq_disable();		\
			ipipe_trace_begin(0x80000000);	\
		}					\
	} while (0)

#define local_irq_enable_hw() do {			\
		if (native_irqs_disabled()) {		\
			ipipe_trace_end(0x80000000);	\
			native_irq_enable();		\
		}					\
	} while (0)

#define local_irq_save_hw(flags) do {			\
		(flags) = native_save_fl();		\
		if ((flags) & X86_EFLAGS_IF) {		\
			native_irq_disable();		\
			ipipe_trace_begin(0x80000001);	\
		}					\
	} while (0)

#define local_irq_restore_hw(flags) do {		\
		if ((flags) & X86_EFLAGS_IF)		\
			ipipe_trace_end(0x80000001);	\
		native_restore_fl(flags);		\
	} while (0)

#else /* !CONFIG_IPIPE_TRACE_IRQSOFF */

#define local_irq_save_hw(flags)	local_irq_save_hw_notrace(flags)
#define local_irq_restore_hw(flags)	local_irq_restore_hw_notrace(flags)
#define local_irq_enable_hw()		local_irq_enable_hw_notrace()
#define local_irq_disable_hw()		local_irq_disable_hw_notrace()

#endif /* CONFIG_IPIPE_TRACE_IRQSOFF */

#define local_save_flags_hw(flags)			\
	do {						\
		(flags) = native_save_fl();		\
	} while (0)

#else

#ifdef CONFIG_X86_64
#define ARCH_LOCKDEP_SYS_EXIT		call lockdep_sys_exit_thunk
#define ARCH_LOCKDEP_SYS_EXIT_IRQ	\
	TRACE_IRQS_ON; \
	sti; \
	SAVE_REST; \
	LOCKDEP_SYS_EXIT; \
	RESTORE_REST; \
	cli; \
	TRACE_IRQS_OFF;

#else
#define ARCH_LOCKDEP_SYS_EXIT			\
	pushl %eax;				\
	pushl %ecx;				\
	pushl %edx;				\
	pushfl;					\
	sti;					\
	call lockdep_sys_exit;			\
	popfl;					\
	popl %edx;				\
	popl %ecx;				\
	popl %eax;

#define ARCH_LOCKDEP_SYS_EXIT_IRQ
#endif

#ifdef CONFIG_TRACE_IRQFLAGS
# ifdef CONFIG_IPIPE
#  ifdef CONFIG_X86_64
#   define TRACE_IRQS_ON			\
	call trace_hardirqs_on_thunk;		\
	pushq %rax;				\
	PER_CPU(ipipe_percpu_darray, %rax);	\
	btrl $0,(%rax);				\
	popq %rax
#   define TRACE_IRQS_OFF			\
	pushq %rax;				\
	PER_CPU(ipipe_percpu_darray, %rax);	\
	btsl $0,(%rax);				\
	popq %rax;				\
	call trace_hardirqs_off_thunk
#  else /* CONFIG_X86_32 */
#   define TRACE_IRQS_ON			\
	call trace_hardirqs_on_thunk;		\
	pushl %eax;				\
	PER_CPU(ipipe_percpu_darray, %eax);	\
	btrl $0,(%eax);				\
	popl %eax
#   define TRACE_IRQS_OFF			\
	pushl %eax;				\
	PER_CPU(ipipe_percpu_darray, %eax);	\
	btsl $0,(%eax);				\
	popl %eax;				\
	call trace_hardirqs_off_thunk
#  endif /* CONFIG_X86_32 */
# else /* !CONFIG_IPIPE */
#  define TRACE_IRQS_ON		call trace_hardirqs_on_thunk;
#  define TRACE_IRQS_OFF	call trace_hardirqs_off_thunk;
# endif /* !CONFIG_IPIPE */
#else
#  define TRACE_IRQS_ON
#  define TRACE_IRQS_OFF
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
#  define LOCKDEP_SYS_EXIT	ARCH_LOCKDEP_SYS_EXIT
#  define LOCKDEP_SYS_EXIT_IRQ	ARCH_LOCKDEP_SYS_EXIT_IRQ
# else
#  define LOCKDEP_SYS_EXIT
#  define LOCKDEP_SYS_EXIT_IRQ
# endif

#endif /* __ASSEMBLY__ */
#endif