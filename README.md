# Parallel LRU cache benchmark

## TODO
- Containers
    - Facebook LRU +
    - Bag LRU
- Python starter +
- Smart key generation +
    - Normal +
    - Uniform +
    - Moving Disjoint +
    - Moving Same +
- DeferredLRU improvements    
    - Multiple recent lists
    - Custom counters
    - LIRS
- Fix LSU3 +
- Pass app to benchmark +

## Experiments
- Revisited
  - Element overhead
    - $ bar
    
  - Find performance
    - $ scalability
      - distribution
        - uniform
        - normal
        
  - Insert/Evict performance
    - scalability
      - $ same sequence
      - $ equidistant
      - $ disjoint
      
  - Real use case
    - LSU
    - $ scalability
    - $ Accesses/Misses/Head
   
  - Hyperparameters
    - pull
    - purge
