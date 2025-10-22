// ConnectivityTest.h - Test harness for connectivity analysis
#pragma once

#include <cstdint>

// Test functions for connectivity analysis
namespace ConnectivityTest {
    
    // Test connectivity analysis on an existing island
    // Prints detailed analysis of connected groups
    void testIslandConnectivity(uint32_t islandID);
    
    // Create a test island with known disconnected groups
    // Returns the island ID for testing
    uint32_t createDisconnectedTestIsland();
    
    // Test split detection by breaking a critical block
    void testBlockBreakSplit(uint32_t islandID);
}
