#include "routing/turns_sound_settings.hpp"

#include "platform/measurement_utils.hpp"

#include "base/string_utils.hpp"

namespace
{
using namespace routing::turns::sound;

}  // namespace

namespace routing
{
namespace turns
{
namespace sound
{
bool Settings::IsValid() const
{
  return m_lengthUnits != LengthUnits::Undefined &&
         m_minNotificationDistanceUnits <= m_maxNotificationDistanceUnits &&
         !m_soundedDistancesUnits.empty() &&
         is_sorted(m_soundedDistancesUnits.cbegin(), m_soundedDistancesUnits.cend());
}

double Settings::ComputeTurnNotificationDistanceUnits(double speedUnitsPerSecond) const
{
  ASSERT(IsValid(), ());

  double turnNotificationDistance = m_notificationTimeSeconds * speedUnitsPerSecond;

  if (turnNotificationDistance < m_minNotificationDistanceUnits)
    turnNotificationDistance = m_minNotificationDistanceUnits;
  if (turnNotificationDistance > m_maxNotificationDistanceUnits)
    turnNotificationDistance = m_maxNotificationDistanceUnits;

  return turnNotificationDistance;
}

uint32_t Settings::RoundByPresetSoundedDistancesUnits(uint32_t turnNotificationUnits) const
{
  ASSERT(IsValid(), ());

  for (auto const distUnits : m_soundedDistancesUnits)
  {
    if (distUnits >= turnNotificationUnits)
      return distUnits;
  }

  ASSERT(false, ("m_soundedDistancesUnits shall contain bigger values."));
  return m_soundedDistancesUnits.empty() ? 0 : m_soundedDistancesUnits.back();
}

double Settings::ConvertMetersPerSecondToUnitsPerSecond(double speedInMetersPerSecond) const
{
  switch (m_lengthUnits)
  {
    case LengthUnits::Undefined:
    default:
      ASSERT(false, ());
      return 0.;
    case LengthUnits::Meters:
      return speedInMetersPerSecond;
    case LengthUnits::Feet:
      return MeasurementUtils::MetersToFeet(speedInMetersPerSecond);
  }
}

double Settings::ConvertUnitsToMeters(double distanceInUnits) const
{
  switch (m_lengthUnits)
  {
    case LengthUnits::Undefined:
    default:
      ASSERT(false, ());
      return 0.;
    case LengthUnits::Meters:
      return distanceInUnits;
    case LengthUnits::Feet:
      return MeasurementUtils::FeetToMeters(distanceInUnits);
  }
}

string DebugPrint(LengthUnits const & lengthUnits)
{
  switch (lengthUnits)
  {
    case LengthUnits::Undefined:
      return "LengthUnits::Undefined";
    case LengthUnits::Meters:
      return "LengthUnits::Undefined";
    case LengthUnits::Feet:
      return "LengthUnits::Feet";
    default:
      stringstream out;
      out << "Unknown LengthUnits value: " << static_cast<int>(lengthUnits);
      return out.str();
  }
}
}  // namespace sound
}  // namespace turns
}  // namespace routing
