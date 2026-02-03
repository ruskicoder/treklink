/**
 * TrekLink Service Locator
 * Core - Static Singleton Registry for Dependency Injection
 * 
 * Provides global access to initialized services without passing pointers.
 * Avoids circular dependencies and simplifies service interactions.
 */

#ifndef SERVICE_LOCATOR_H
#define SERVICE_LOCATOR_H

// Forward declarations
class LoRaDriver;
class SecurityService;
class PRFHService;
class MeshService;

/**
 * Service Locator (Static Singleton Pattern)
 * 
 * Usage:
 *   ServiceLocator::getMesh()->sendPing();
 *   ServiceLocator::getSecurity()->encrypt(...);
 */
class ServiceLocator {
public:
    // ========== Getters ==========
    
    static LoRaDriver* getLora() { return loraDriver; }
    static SecurityService* getSecurity() { return securityService; }
    static PRFHService* getPRFH() { return prfhService; }
    static MeshService* getMesh() { return meshService; }
    
    // ========== Initialization ==========
    
    /**
     * Register LoRa driver
     * Call from setup() after LoRaDriver::begin()
     */
    static void registerLora(LoRaDriver* lora) { loraDriver = lora; }
    
    /**
     * Register Security service
     * Call from setup() after SecurityService::begin()
     */
    static void registerSecurity(SecurityService* security) { securityService = security; }
    
    /**
     * Register PRFH service
     * Call from setup() after PRFHService::begin()
     */
    static void registerPRFH(PRFHService* prfh) { prfhService = prfh; }
    
    /**
     * Register Mesh service
     * Call from setup() after MeshService::begin()
     */
    static void registerMesh(MeshService* mesh) { meshService = mesh; }

private:
    // Static service pointers
    static LoRaDriver* loraDriver;
    static SecurityService* securityService;
    static PRFHService* prfhService;
    static MeshService* meshService;
};

#endif // SERVICE_LOCATOR_H
