#include "tcp-dcerl.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpDcerl");
NS_OBJECT_ENSURE_REGISTERED (TcpDcerl);

// ================= TYPE =================
TypeId
TcpDcerl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpDcerl")
    .SetParent<TcpCongestionOps> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpDcerl> ();
  return tid;
}

// ================= CONSTRUCTOR =================
TcpDcerl::TcpDcerl ()
  : TcpCongestionOps (),
    m_minRtt (1e9),
    m_avgRtt (0),
    m_Bi (1.0),
    m_Be (1.0),
    m_dupAckCount (0),
    m_gamma (2)   // γ
{
}

TcpDcerl::TcpDcerl (const TcpDcerl& sock)
  : TcpCongestionOps (sock),
    m_minRtt (sock.m_minRtt),
    m_avgRtt (sock.m_avgRtt),
    m_Bi (sock.m_Bi),
    m_Be (sock.m_Be),
    m_dupAckCount (sock.m_dupAckCount),
    m_gamma (sock.m_gamma)
{
}

TcpDcerl::~TcpDcerl () {}

std::string
TcpDcerl::GetName () const
{
  return "TcpDcerl";
}

Ptr<TcpCongestionOps>
TcpDcerl::Fork ()
{
  return CopyObject<TcpDcerl> (this);
}

// ================= RTT UPDATE =================
void
TcpDcerl::UpdateRtt (double rtt)
{
  if (rtt <= 0) return;

  m_minRtt = std::min(m_minRtt, rtt);
  m_avgRtt = 0.9 * m_avgRtt + 0.1 * rtt;
}

// ================= MAIN LOGIC =================
void
TcpDcerl::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  uint32_t cwnd = tcb->m_cWnd.Get ();
  uint32_t MSS = tcb->m_segmentSize;

  double rtt = tcb->m_lastRtt.Get ().GetSeconds ();
  if (rtt <= 0) rtt = 0.001;

  UpdateRtt(rtt);

  double T = m_minRtt;
  if (T <= 0) T = rtt;

  // ================= DUP ACK DETECTION =================
  if (segmentsAcked == 0)
    m_dupAckCount++;
  else
    m_dupAckCount = 0;

  // ================= TIMEOUT DETECTION =================
  // Approximation: long period without ACKs
  if (m_dupAckCount > 20)
  {
    NS_LOG_INFO("Timeout detected");

    uint32_t newCwnd = m_gamma * MSS;
    tcb->m_cWnd = newCwnd;
    tcb->m_ssThresh = 65535;

    m_dupAckCount = 0;
    return;
  }

  // ================= DCERL+ COMPUTATION =================

  double lambda = rtt / T;

  double li = (rtt - T) * m_Bi;
  double le = (rtt - T) * m_Be;

  double lmax = std::max(li, le);
  double N = lambda * lmax;

  if (li <= 1e-6)
    li = 1e-6;

  double R = N / li;

  // ================= CWND UPDATE =================

  if (li < N)
  {
    double inc = std::max(1.0, R);
    cwnd += static_cast<uint32_t>(inc * MSS);
  }
  else
  {
    tcb->m_ssThresh = std::max(cwnd / 2, 2 * MSS);

    double dec = std::max(1.0, 1.0 / R);
    uint32_t reduction = static_cast<uint32_t>(dec * MSS);

    if (cwnd > reduction)
      cwnd -= reduction;
    else
      cwnd = MSS;
  }

  // ================= DUP ACK EVENT (PURE ALGORITHM) =================
  if (m_dupAckCount == 3)
  {
    NS_LOG_INFO("3 Dup ACKs → applying DCERL rule");

    if (li < N)
    {
      double inc = std::max(1.0, R);
      cwnd += static_cast<uint32_t>(inc * MSS);
    }
    else
    {
      tcb->m_ssThresh = std::max(cwnd / 2, 2 * MSS);

      double dec = std::max(1.0, 1.0 / R);
      uint32_t reduction = static_cast<uint32_t>(dec * MSS);

      if (cwnd > reduction)
        cwnd -= reduction;
      else
        cwnd = MSS;
    }

    m_dupAckCount = 0;
  }

  tcb->m_cWnd = cwnd;

  NS_LOG_INFO("DCERL+ cwnd: " << cwnd
                << " R: " << R
                << " N: " << N
                << " li: " << li);
}

// ================= SSTHRESH =================
uint32_t
TcpDcerl::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  return std::max(2 * tcb->m_segmentSize, bytesInFlight / 2);
}

} // namespace ns3
