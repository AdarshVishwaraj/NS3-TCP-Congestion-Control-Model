#ifndef TCP_DCERL_H
#define TCP_DCERL_H

#include "ns3/tcp-congestion-ops.h"

namespace ns3 {

class TcpDcerl : public TcpCongestionOps
{
public:
  static TypeId GetTypeId (void);

  TcpDcerl ();
  TcpDcerl (const TcpDcerl& sock);
  virtual ~TcpDcerl ();

  virtual std::string GetName () const override;

  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;

  virtual Ptr<TcpCongestionOps> Fork () override;

private:
  // RTT tracking
  double m_minRtt;
  double m_avgRtt;

  // Algorithm parameters
  double m_Bi;
  double m_Be;

  // ACK tracking
  uint32_t m_dupAckCount;

  // Initial cwnd parameter γ
  uint32_t m_gamma;

  void UpdateRtt (double rtt);
};

} // namespace ns3

#endif
