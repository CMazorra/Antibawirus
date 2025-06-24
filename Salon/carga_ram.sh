#!/usr/bin/env bash
exec -a "testeo de memoria" bash <<'EOF'
echo "🔧 Reservando 120 MB de RAM…"
buffer=$(dd if=/dev/zero bs=1M count=120 2>/dev/null | tr '\0' 'A')
echo "📌 Proceso 'testeo de memoria' usando 120 MB. Ejecutándose 30 s…"
sleep 30
unset buffer
echo "✅ Memoria liberada. Saliendo."
EOF
