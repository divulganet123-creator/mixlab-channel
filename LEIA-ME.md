# MixLab Channel (VST3, Windows)

Canal de mixagem para uma pista: passa-altas, EQ de 3 bandas, de-esser, compressor, delay, reverb, volume de saída e limitador.

## 1. Compilar sem instalar nada (GitHub Actions)

1. Crie uma conta gratuita em github.com e um repositório novo (pode ser privado).
2. Envie todo o conteúdo desta pasta para o repositório, mantendo as pastas, inclusive `.github/workflows`. Pelo site: **Add file > Upload files** e arraste tudo.
3. Abra a aba **Actions**, escolha **Compilar VST3 (Windows)** e clique em **Run workflow**. A compilação leva uns 10 a 15 minutos.
4. Quando terminar, abra a execução e baixe o arquivo **MixLab-Channel-VST3** na seção Artifacts. Descompacte: dentro está a pasta `MixLab Channel.vst3`.

## 2. Instalar

Copie a pasta `MixLab Channel.vst3` para:

`C:\Program Files\Common Files\VST3`

## 3. Usar no Ableton Live (versão 11 ou mais nova)

1. Em **Preferências > Plug-ins**, ative **Usar pastas do sistema de plug-ins VST3** e clique em **Reescanear**.
2. No navegador, abra **Plug-ins > VST3** e arraste **MixLab Channel** para uma pista.

## Observações

- A interface é a genérica do JUCE, com um controle por parâmetro. Os nomes não têm acento.
- O plugin processa em tempo real. O "Tirar ruído" e o "Auto-mix com IA" da página web não fazem parte dele.
- Este código não foi compilado nem testado antes de ser entregue. Se a compilação falhar, abra a execução com erro na aba Actions e envie o texto do erro para correção.
