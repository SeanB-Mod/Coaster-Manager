/* GameItem ID persists across a save/load; ECS handles are session-local. */
static int retake_identity_matches(unsigned long long source, long long definition,
                                   unsigned long long current, long long currentDefinition,
                                   const unsigned *entity) {
    return source && definition && source == current && definition == currentDefinition && entity &&
           entity[0] && entity[1];
}
