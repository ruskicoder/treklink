/**
 * TrekLink Service Locator Implementation
 * Core - Static Singleton Registry
 */

#include "core/service_locator.h"

// Initialize static pointers to nullptr
LoRaDriver* ServiceLocator::loraDriver = nullptr;
SecurityService* ServiceLocator::securityService = nullptr;
PRFHService* ServiceLocator::prfhService = nullptr;
MeshService* ServiceLocator::meshService = nullptr;
