#ifndef SGL_SIGRIND_SHMEM_H
#define SGL_SIGRIND_SHMEM_H

#include "ShMemData.h"
#include <memory>

namespace sgl
{
namespace sigrind
{

class ShMem
{
	SigrindSharedData* shared_mem;
public:
	ShMem();
	~ShMem();

	/* Get all events from Sigrind until it's signaled to stop by Sigrind */
	/* FIXME ML: Sigil2 can hang here if Sigrind exits unexpectedly before 
	 * signaling ShMem to finish. Implementing a named pipe allows us
	 * to detect this */
	void readFromSigrind();

private:
	void flush_sigrind_to_sigil(unsigned int size);
};	

}; //end namespace sigrind
}; //end namespace sgl

#endif