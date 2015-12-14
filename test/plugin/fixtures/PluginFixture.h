#ifndef PROXYFIXTURE_H
#define PROXYFIXTURE_H

#include <dnp3hammer.h>
#include <utility>
#include <vector>
#include <string>


// plugin callbacks - use these to drive events with the fixture
int  cb_link_frame(void *env, const DNP3_Frame *frame, const uint8_t *buf, size_t len);
void cb_transport_segment(void *env, const DNP3_Segment *segment);
void cb_transport_payload(void *env, const uint8_t *s, size_t n);
void cb_app_invalid(void *env, DNP3_ParseError e);
void cb_app_fragment(void *env, const DNP3_Fragment *fragment, const uint8_t *buf, size_t len);

// corresponding event enums
enum class Event
{
    LINK_FRAME,
    TRANS_SEGMENT,
    TRANS_PAYLOAD,
    APP_INVALID,
    APP_FRAG
};


typedef std::pair<const uint8_t*, size_t> slice_t;

class PluginFixture
{
    public:
        PluginFixture();
        ~PluginFixture();

        bool Parse(const std::string& hex);

        bool CheckEvents(std::initializer_list<Event> expected) const;

        std::vector<Event> events;

    private:
        StreamProcessor* m_plugin;
};

#endif //PROXYFIXTURE_H
