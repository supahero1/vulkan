#include "../include/vulkan.h"

#include <signal.h>

int
main(
	void
	)
{
	VulkanInit();

	VulkanRun();

	VulkanFree();

	return 0;
}
