import { useEffect, useRef, useCallback } from 'react';

export type WsStatus = 'connecting' | 'connected' | 'disconnected';

export type WsMessageHandler = (event: MessageEvent) => void;

export function useWebSocket(
  url: string,
  onMessage: WsMessageHandler,
  onStatusChange?: (status: WsStatus) => void
): void {
  const wsRef = useRef<WebSocket | null>(null);
  const onMessageRef = useRef(onMessage);
  const onStatusRef = useRef(onStatusChange);
  const retryDelayRef = useRef(1000);
  const unmountedRef = useRef(false);

  // Keep refs current without triggering reconnect
  useEffect(() => { onMessageRef.current = onMessage; });
  useEffect(() => { onStatusRef.current = onStatusChange; });

  const connect = useCallback(() => {
    if (unmountedRef.current) return;

    onStatusRef.current?.('connecting');
    const ws = new WebSocket(url);
    wsRef.current = ws;

    ws.onopen = () => {
      retryDelayRef.current = 1000;
      onStatusRef.current?.('connected');
    };

    ws.onmessage = (event) => {
      onMessageRef.current(event);
    };

    ws.onclose = () => {
      if (unmountedRef.current) return;
      onStatusRef.current?.('disconnected');
      // Exponential backoff, cap at 30s
      const delay = retryDelayRef.current;
      retryDelayRef.current = Math.min(delay * 2, 30000);
      setTimeout(connect, delay);
    };

    ws.onerror = () => {
      ws.close();
    };
  }, [url]);

  useEffect(() => {
    unmountedRef.current = false;
    connect();
    return () => {
      unmountedRef.current = true;
      wsRef.current?.close();
    };
  }, [connect]);
}
