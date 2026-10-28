# Validação da prévia 0.2.0

Verificação local em 27/10/2026, macOS Intel (x86_64), JUCE 9.

- **40 verificações automatizadas passaram**, incluindo o nome NF Vocal Fine no host.
- Detecção e correção de áudio em 44,1, 48 e 96 kHz.
- Latência de 120 ms confirmada por impulso, no áudio processado e no original.
- Estéreo, silêncio após reset, preservação de vibrato, seleção de escala e notas.
- Carregamento VST3, 19 controles mais bypass, recuperação de estado e abertura da interface.
- Assinatura local ad-hoc verificada após a geração do manifesto.
- Interface conferida visualmente, com o logo original incorporado ao binário.
- Motor de áudio idêntico ao da prévia 0.1.0; as mudanças desta versão são nome e interface.
- Os identificadores VST3 de processador e controlador foram preservados na renomeação. Não instale as duas prévias na mesma pasta de busca.

Os testes automatizados usam sinais sintéticos e um host de teste JUCE. Não são certificação de compatibilidade com todas as DAWs nem confirmação de naturalidade em voz real. A aprovação auditiva do usuário precede qualquer integração nativa na Mixa Studio.

Não há gravações do usuário neste repositório.
