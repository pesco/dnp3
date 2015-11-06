#ifndef PROXYFIXTURE_H
#define PROXYFIXTURE_H

#include <dnp3hammer/plugin.h>
#include <utility>
#include <vector>

typedef std::pair<const uint8_t*, size_t> slice_t;

class ProxyFixture
{
    public:
        ProxyFixture();
        ~ProxyFixture();

        // the output that gets written by the plugin
        std::vector<slice_t> writes;

    private:

        static void QueueOutput(void *env, const uint8_t *buf, size_t n);

        Plugin* m_plugin;

};

#endif //PROXYFIXTURE_H
