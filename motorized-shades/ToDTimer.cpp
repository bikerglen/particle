#include "Particle.h"
#include "ToDTimer.h"

ToDTimer::ToDTimer (void)
{
    this->valid = false;
    this->timerTime = 0;
    this->lastChecked = 0;
}


void ToDTimer::SetTimer (time_t now, int hour, int minute)
{
  	this->SetTimer (now, hour, minute, 0);
}


void ToDTimer::SetTimer (time_t now, int hour, int minute, int offset)
{
    struct tm t;

  	t.tm_year = Time.year(now) - 1900;
  	t.tm_mon = Time.month(now) - 1;
  	t.tm_mday = Time.day(now);
  	t.tm_hour = hour;
  	t.tm_min = minute;
  	t.tm_sec = 0;
  	t.tm_isdst = Time.isDST() ? 1 : 0;

    this->valid = true;
  	this->timerTime = mktime (&t) + 60*offset;
    this->lastChecked = now;
}


bool ToDTimer::CheckTimer (time_t now)
{
  	bool result = false;

  	if (this->valid) {
        if ((now >= this->timerTime) && (this->lastChecked < this->timerTime)) {
      			result = true;
      			this->valid = false;
            Serial.printf ("Timer triggered!\n\r");
            Serial.printf ("now: %s\n\r", Time.format (now, TIME_FORMAT_DEFAULT).c_str());
            Serial.printf ("last: %s\n\r", Time.format (this->lastChecked, TIME_FORMAT_DEFAULT).c_str());
            Serial.printf ("timer: %s\n\r", Time.format (this->timerTime, TIME_FORMAT_DEFAULT).c_str());
  		  }
  	}
  	this->lastChecked = now;

  	return result;
}


String ToDTimer::GetTimeString (void)
{
    return Time.format (this->timerTime, TIME_FORMAT_DEFAULT);
}
