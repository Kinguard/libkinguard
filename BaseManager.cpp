#include "BaseManager.h"

namespace KGP
{

BaseManager::BaseManager() = default;

string BaseManager::StrError()
{
	return this->global_error;
}

BaseManager::~BaseManager() = default;

} // Namespace OPI
