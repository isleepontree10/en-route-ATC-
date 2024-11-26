
inline const char * const BoolToString(bool b)
{
  return b ? "true" : "false";
}


inline const bool vectorWithinAirspaceBounds(Vec3 v){
	pair<float,float> xBound = make_pair(MIN_AIRSPACE_X_BOUND,MAX_AIRSPACE_X_BOUND);
	pair<float,float> yBound = make_pair(MIN_AIRSPACE_Y_BOUND,MAX_AIRSPACE_Y_BOUND);
	pair<float,float> zBound = make_pair(MIN_AIRSPACE_Z_BOUND,MAX_AIRSPACE_Z_BOUND);
	return (
			v.x >= xBound.first &&
			v.x <= xBound.second &&
			v.y >= yBound.first &&
			v.y <= yBound.second &&
			v.z >= zBound.first &&
			v.z <= zBound.second);
}

inline const void printCurrentTime() {
	char s[1000];

	time_t t = time(NULL);
	struct tm *p = localtime(&t);

	strftime(s, 1000, "%A, %B %d %Y %HH %MM %SS", p);

	printf("%s ", s);
}
