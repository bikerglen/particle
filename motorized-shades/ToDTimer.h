class ToDTimer {
  
  public:
    ToDTimer (void);
    void SetTimer (time_t now, int hour, int minute);
    void SetTimer (time_t now, int hour, int minute, int offset);
    bool CheckTimer (time_t now);
    String GetTimeString (void);

  private:
    bool valid;
    time_t timerTime;
    time_t lastChecked;
};
