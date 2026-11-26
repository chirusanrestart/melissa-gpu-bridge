# Melissa GPU Bridge

Diagnóstico Vulkan nativo para Android, com execução de Compute Shader no app e uma interface de linha de comando independente para Termux.

## Arquitetura

- **App Android:** interface Kotlin para diagnóstico do dispositivo e execução do teste Vulkan Compute.
- **Núcleo compartilhado:** C++17 com Vulkan, usado tanto pelo app via JNI quanto pelo executável do Termux.
- **Termux CLI:** executável nativo `mgpu`; não precisa deixar o app Android aberto.
- **Shaders:** `vector.comp` é compilado para SPIR-V no GitHub Actions e embutido no binário.
- **Drivers:** o projeto usa o loader e o driver Vulkan fornecidos pelo Android. Nenhum driver de fabricante é empacotado.

O teste executa `output = input * 2 + 1` em um buffer de floats e valida cada elemento retornado.

## Comandos do Termux

```bash
mgpu info
mgpu devices
mgpu test
mgpu test 1048576
mgpu test 1048576 --json
mgpu bench
```

- `info`: mostra a disponibilidade do Vulkan e as propriedades da GPU.
- `devices`: lista os dispositivos Vulkan encontrados.
- `test [quantidade]`: executa o Compute Shader com 1 a 1.048.576 elementos. O padrão é 256.
- `bench`: executa os tamanhos 256, 4K, 64K, 256K e 1M, em sequência.
- `--json`: retorna o resultado em JSON para uso por scripts.

Entradas inválidas são rejeitadas antes da inicialização do Vulkan. O CLI retorna código de saída diferente de zero quando a inicialização Vulkan ou a validação do Compute falha.

## Instalar no Termux

1. Abra o repositório no GitHub e entre em **Actions**.
2. Abra a execução do workflow associada ao PR **Add native Vulkan compute shader test**.
3. Baixe o artefato **melissa-gpu-bridge-termux** e extraia o ZIP.
4. No Termux, instale o loader Android e o utilitário de extração:

   ```bash
   pkg update
   pkg install vulkan-loader-android unzip
   ```

5. Entre na pasta extraída e execute o instalador:

   ```bash
   cd /caminho/da/pasta-extraida
   bash install.sh
   ```

6. Confira o dispositivo:

   ```bash
   mgpu info
   mgpu test 1048576
   ```

O instalador escolhe automaticamente o binário correspondente à arquitetura do ambiente Termux (`aarch64` ou `arm`) e copia o executável para `$PREFIX/bin/mgpu`. Não exige root.

## Requisitos e limitações

- Android com suporte Vulkan e uma fila que exponha `VK_QUEUE_COMPUTE_BIT`.
- Termux compatível com ARM64 (`aarch64`) ou ARMv7 (`arm`).
- Pacote `vulkan-loader-android` instalado no Termux para expor o loader Vulkan do sistema Android.
- O dispositivo precisa permitir que o loader do Android alcance um driver que exponha Compute. A presença do loader, por si só, não garante que a GPU tenha uma fila Compute disponível.
- O heap local informado pelo Vulkan é uma propriedade anunciada pelo driver, não uma confirmação de RAM dedicada.

## Compilar localmente

Para o APK, abra o projeto no Android Studio ou execute:

```bash
./gradlew assembleDebug
```

Para executar os testes do parser e do CLI no host:

```bash
cmake -S termux -B build/cli-host -DBUILD_TESTING=ON
cmake --build build/cli-host --parallel
ctest --test-dir build/cli-host --output-on-failure
```

Os executáveis Termux são compilados no GitHub Actions com Android NDK 27.2.12479018 para `armeabi-v7a` e `arm64-v8a`.

## Segurança

O CLI aceita apenas comandos e opções explicitamente definidos. Ele não recebe nem executa comandos shell arbitrários. O binário depende do loader Vulkan do sistema Android e não instala, substitui ou altera drivers do dispositivo.
