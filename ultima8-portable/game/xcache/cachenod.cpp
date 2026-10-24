// name: ..\XCACHE\CACHENOD
// Ported from CACHENOD.ASM.
// CacheNodePtr's linked-list and header field code, written in assembly (.386 flat
// offsets into the cache memory block at CacheNodePtr::memBase).
//
// Every routine takes the node's 32-bit offset and works on the 29-byte CacheNode headers
// at memBase + offset (the assembly loaded memBase's selector into ds and used the offsets
// as 32-bit addresses). The loads and stores are done in the assembly's order, which
// matters where a node is its own neighbour (the dummy nodes) or where a neighbour's header
// lies inside memory that is about to be overwritten (shiftUpAsm, shiftFreeBlock).

#include "u8port.h"

#include "cache.h"
#include "handler.h"

void *CacheNodePtr::memBase = 0;

// The header at offset o of the cache.
static inline CacheNode *node_at(uint32_t o)
{
	return (CacheNode *)CacheNodePtr::getPtr(o);
}

// Links this node in as the most recently used: just behind blockRoot on its less side.
void CacheNodePtr::freeSpaceToBlockMru(void)
{
	uint32_t ebx = offset;
	uint32_t eax = blockRoot.offset;
	node_at(ebx)->more = eax;
	node_at(ebx)->less = node_at(eax)->less;
	node_at(eax)->less = ebx;
	eax = node_at(ebx)->less;
	node_at(eax)->more = ebx;
}

// Moves this node to the most recently used end of the MRU thread.
void CacheNodePtr::makeMru(void)
{
	uint32_t eax = offset;
	unlinkFromMruThread();
	uint32_t ebx = blockRoot.offset;
	node_at(eax)->less = node_at(ebx)->less;
	node_at(eax)->more = ebx;
	node_at(ebx)->less = eax;
	uint32_t ecx = node_at(eax)->less;
	node_at(ecx)->more = eax;
}

// Takes this node out of the MRU (or free) thread. The assembly left the node's offset in
// eax, so the low word of the offset is what comes back in ax.
int16_t CacheNodePtr::unlinkFromMruThread(void)
{
	uint32_t eax = offset;
	uint32_t ebx = node_at(eax)->less;
	uint32_t ecx = node_at(eax)->more;
	node_at(ecx)->less = ebx;
	ebx = node_at(eax)->more;
	ecx = node_at(eax)->less;
	node_at(ecx)->more = ebx;
	return (int16_t)eax;
}

// Takes this node out of the block thread.
void CacheNodePtr::unlinkFromBlockThread(void)
{
	uint32_t eax = offset;
	uint32_t ebx = node_at(eax)->next;
	uint32_t ecx = node_at(eax)->prev;
	node_at(ecx)->next = ebx;
	ebx = node_at(eax)->prev;
	ecx = node_at(eax)->next;
	node_at(ecx)->prev = ebx;
}

// Links this node into the block thread after node.
void CacheNodePtr::blockThreadInsertAfter(CacheNodePtr node)
{
	uint32_t eax = offset;
	uint32_t ebx = node.offset;
	uint32_t ecx = node_at(ebx)->next;
	node_at(eax)->next = ecx;
	node_at(eax)->prev = ebx;
	node_at(ecx)->prev = eax;
	node_at(ebx)->next = eax;
}

// Puts this node in node's place in the MRU (or free) thread.
void CacheNodePtr::mruThreadReplace(CacheNodePtr node)
{
	uint32_t eax = offset;
	uint32_t ebx = node.offset;
	node_at(eax)->less = node_at(ebx)->less;
	node_at(eax)->more = node_at(ebx)->more;
	uint32_t ecx = node_at(eax)->less;
	node_at(ecx)->more = eax;
	ecx = node_at(eax)->more;
	node_at(ecx)->less = eax;
}

// Links this node into the free chain, just behind freeRoot on its less side.
void CacheNodePtr::addToFreeChain(void)
{
	uint32_t eax = offset;
	uint32_t ebx = freeRoot.offset;
	node_at(eax)->less = node_at(ebx)->less;
	node_at(eax)->more = ebx;
	uint32_t ecx = node_at(ebx)->less;
	node_at(ecx)->more = eax;
	node_at(ebx)->less = eax;
}

// Makes this node's block thread skip node (the node after it, being merged in).
void CacheNodePtr::setMergeLink(CacheNodePtr node)
{
	uint32_t eax = offset;
	uint32_t ebx = node.offset;
	node_at(eax)->next = node_at(ebx)->next;
	uint32_t ecx = node_at(eax)->next;
	node_at(ecx)->prev = eax;
}

// Points this node's four neighbours at node instead (the node is moving there).
void CacheNodePtr::reset(CacheNodePtr node)
{
	uint32_t eax = offset;
	uint32_t ebx = node.offset;
	uint32_t ecx = node_at(eax)->prev;
	node_at(ecx)->next = ebx;
	ecx = node_at(eax)->next;
	node_at(ecx)->prev = ebx;
	ecx = node_at(eax)->less;
	node_at(ecx)->more = ebx;
	ecx = node_at(eax)->more;
	node_at(ecx)->less = ebx;
}

// This node is moving to node, over the free node in front of it: the block thread around
// the pair and the MRU thread around this node are pointed at node, and the free node is
// taken out of the free chain.
void CacheNodePtr::shiftUpSet(CacheNodePtr node)
{
	uint32_t eax = offset;
	uint32_t ebx = node.offset;
	uint32_t ecx = node_at(eax)->prev;
	uint32_t edx = node_at(ecx)->prev;
	node_at(edx)->next = ebx;
	ecx = node_at(eax)->next;
	node_at(ecx)->prev = ebx;
	ebx = node_at(eax)->prev;
	ecx = node_at(ebx)->more;
	edx = node_at(ebx)->less;
	node_at(edx)->more = ecx;
	ecx = node_at(ebx)->less;
	edx = node_at(ebx)->more;
	node_at(edx)->less = ecx;
	ebx = node.offset;
	ecx = node_at(eax)->less;
	node_at(ecx)->more = ebx;
	ecx = node_at(eax)->more;
	node_at(ecx)->less = ebx;
}

// Builds the empty cache: blockRoot (a dummy, also the MRU thread's root), freeRoot (a
// dummy, the free chain's root) and one free node of freeBytes after them, two headers
// into the cache. The sum fields are left as they are.
void CacheNodePtr::setDummyNodes(void)
{
	uint32_t eax = cache.offset + 0x3A;
	uint32_t ebx = blockRoot.offset;
	CacheNode *n = node_at(ebx);
	n->next = eax;
	n->prev = eax;
	n->less = ebx;
	n->more = ebx;
	n->type = (CacheNodeType)0xFF;
	n->handle = 0xFFFF;
	n->size = 0;
	n->sizeDiff = 0;

	n = node_at(eax);
	n->next = blockRoot.offset;
	n->prev = blockRoot.offset;
	n->less = freeRoot.offset;
	n->more = freeRoot.offset;
	n->type = (CacheNodeType)0xFF;
	n->handle = 0xFFFE;
	n->size = freeBytes;
	n->sizeDiff = 0;

	ebx = freeRoot.offset;
	n = node_at(ebx);
	n->next = ebx;
	n->prev = ebx;
	n->less = eax;
	n->more = eax;
	n->type = (CacheNodeType)0xFF;
	n->handle = 0xFFFF;
	n->size = 0;
	n->sizeDiff = 0;

	largestFreeNode.offset = eax;
}

int32_t CacheNodePtr::getSum(void)
{
	return node_at(offset)->sum;
}

int32_t CacheNodePtr::getSize(void)
{
	return node_at(offset)->size;
}

int32_t CacheNodePtr::getLess(void)
{
	return node_at(offset)->less;
}

int32_t CacheNodePtr::getMore(void)
{
	return node_at(offset)->more;
}

int32_t CacheNodePtr::getNext(void)
{
	return node_at(offset)->next;
}

int32_t CacheNodePtr::getPrev(void)
{
	return node_at(offset)->prev;
}

// The handle word as an int (FREE_HANDLE comes back as -2): compare it as uint16_t.
int16_t CacheNodePtr::getHandle(void)
{
	return (int16_t)node_at(offset)->handle;
}

uint16_t CacheNodePtr::getSizeDiff(void)
{
	return node_at(offset)->sizeDiff;
}

int16_t CacheNodePtr::getType(void)
{
	return (int16_t)node_at(offset)->type;
}

void CacheNodePtr::setSize(int32_t size)
{
	node_at(offset)->size = size;
}

void CacheNodePtr::setSum(int32_t sum)
{
	node_at(offset)->sum = sum;
}

void CacheNodePtr::setNext(CacheNodePtr node)
{
	node_at(offset)->next = node.offset;
}

void CacheNodePtr::setPrev(CacheNodePtr node)
{
	node_at(offset)->prev = node.offset;
}

void CacheNodePtr::setType(CacheNodeType type)
{
	node_at(offset)->type = type;
}

void CacheNodePtr::setSizeDiff(uint8_t diff)
{
	node_at(offset)->sizeDiff = diff;
}

void CacheNodePtr::setHandle(uint16_t handle)
{
	node_at(offset)->handle = handle;
}

// Moves this node distance bytes towards the start of the cache: repoints its handle,
// relinks its neighbours to the new place, then copies getSize() bytes from the old
// header to the new one (header included, so the last NODE_HEADER bytes of data are not
// moved). Nothing calls it; CACHE.C's shiftUp does this job.
//
// The copy runs forwards a word at a time with an odd byte last (rep movsw, rep movsb),
// as in the assembly. The value returned was memBase's selector in dx and the moved node's
// type word in ax, which is memBase + type here.
//
// The assembly stored the new handle value at ds:2A40h + handle * 4: 2A40h is where
// CacheHandler::handles (a pointer to the table, not the table) sat in DGROUP, so it wrote
// over that pointer and the data after it. Writing that is not possible here; this stores
// into the handle table, as CACHE.C does.
void *CacheNodePtr::shiftUpAsm(int32_t distance)
{
	uint32_t ebx = offset;
	uint32_t to = ebx - (uint32_t)distance;
	uint16_t handle = node_at(ebx)->handle;
	if (handle < NO_HANDLE)
		CacheHandler::handles[handle] = to + NODE_HEADER;
	reset(CacheNodePtr(to));
	uint32_t count = (uint32_t)node_at(ebx)->size;
	uint8_t *dest = (uint8_t *)getPtr(to);
	uint8_t *src = (uint8_t *)getPtr(ebx);
	for (uint32_t i = count >> 1; i != 0; i--)
	{
		uint8_t lo = src[0];
		uint8_t hi = src[1];
		dest[0] = lo;
		dest[1] = hi;
		dest += 2;
		src += 2;
	}
	if (count & 1)
		*dest = *src;
	uint16_t type = (uint16_t)node_at(to)->type;
	return (char *)memBase + type;
}

// Moves this free node's header distance bytes towards the start of the cache and grows
// the node by distance bytes: the gap left by the nodes shifted up in front of it.
void CacheNodePtr::shiftFreeBlock(int32_t distance)
{
	uint32_t ebx = offset;
	uint32_t eax = ebx - (uint32_t)distance;
	reset(CacheNodePtr(eax));
	int32_t size = node_at(ebx)->size + distance;
	// rep movsb, forwards.
	uint8_t *dest = (uint8_t *)getPtr(eax);
	uint8_t *src = (uint8_t *)getPtr(ebx);
	for (int16_t i = 0; i < NODE_HEADER; i++)
		dest[i] = src[i];
	node_at(eax)->size = size;
}
