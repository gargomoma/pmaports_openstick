"""Offline checks for the legacy NAT backend used by the Honor 6X 4.4 port."""
import pathlib, subprocess, unittest
ROOT=pathlib.Path(__file__).parent
class HotspotNatTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.source=(ROOT/'honor6x-hotspot-worker').read_text()
  recipe=ROOT/'APKBUILD'
  if not recipe.exists(): recipe=ROOT.parent/'APKBUILD'
  cls.recipe=recipe.read_text()
 def test_worker_shell(self):
  r=subprocess.run(['/bin/sh','-n',str(ROOT/'honor6x-hotspot-worker')],capture_output=True,text=True); self.assertEqual(r.returncode,0,r.stderr)
 def test_dependency(self): self.assertRegex(self.recipe,r'(?m)^iptables-legacy$')
 def test_requires_legacy_frontend(self):
  for x in ('iptables-legacy','command -v "$command"','iptables-legacy -t nat -L'): self.assertIn(x,self.source)
 def test_isolated_chains(self):
  for x in ('filter_chain=H6X_HOTSPOT_FWD','nat_chain=H6X_HOTSPOT_NAT','iptables-legacy -N "$filter_chain"','iptables-legacy -t nat -N "$nat_chain"','iptables-legacy -I FORWARD 1 -j "$filter_chain"','iptables-legacy -t nat -I POSTROUTING 1 -j "$nat_chain"','-j MASQUERADE'): self.assertIn(x,self.source)
 def test_forward_return_path(self):
  self.assertIn('-m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT',self.source); self.assertIn('-s 192.168.51.0/24 -j ACCEPT',self.source)
 def test_cleanup(self):
  for x in ('iptables-legacy -D FORWARD -j "$filter_chain"','iptables-legacy -t nat -D POSTROUTING -j "$nat_chain"','iptables-legacy -X "$filter_chain"','iptables-legacy -t nat -X "$nat_chain"'): self.assertIn(x,self.source)
if __name__=='__main__': unittest.main()
