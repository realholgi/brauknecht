#ifndef _GLOBAL_H
#define _GLOBAL_H

bool isDebugEnabled()
{
#ifdef DEBUG
  return true;
#endif // DEBUG
  return false;
}

// generic serial output
template <typename T>
void SerialOut(const T aValue, bool newLine = true)
{
  if (!isDebugEnabled())
    return;
  Serial.print(aValue);
  if (newLine)
    Serial.print("\n");
}

void SerialOut() {
  SerialOut("");
}


#endif
